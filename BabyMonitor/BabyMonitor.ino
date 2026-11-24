
#include <Arduino_MKRIoTCarrier.h>
#include "NetworkManager.h"
#include "SensorManager.h"
#include "ButtonManager.h"
#include "MenuManager.h"
#include "EventManager.h"
#include <ArduinoJson.h>
#include <RTCZero.h>
#include <time.h>
#include <math.h>

// Temporary event test mode. Set true and upload again to restore measurements.
const bool MEASUREMENTS_ENABLED = true;

MKRIoTCarrier carrier;
const float TEMPERATURE_OFFSET_C = -2.0f;
SensorManager sensors(carrier, TEMPERATURE_OFFSET_C);
ButtonManager buttons(carrier);
MenuManager menu(carrier);
EventManager events;

NetworkManager network;
RTCZero rtc;

bool timeReady = false;
bool clockAttempted = false;
unsigned long lastClockAttempt = 0;

uint32_t sampleEpoch = 0;

const unsigned long TELEMETRY_INTERVAL_MS = 20000UL;
unsigned long lastTelemetry = 0;
void serviceLocalTasks();


unsigned long lastSensorDiagnostic = 0;
unsigned long lastTelemetryDiagnostic = 0;

void reportSensorStatus(unsigned long now) {
  if (!MEASUREMENTS_ENABLED) return;
  if (now - lastSensorDiagnostic < 5000UL) return;
  lastSensorDiagnostic = now;

  Serial.print("[SENSOR] temperature_c=");
  Serial.print(sensors.getTemperature(), 1);
  Serial.print(" | humidity_pct=");
  Serial.print(sensors.getHumidity(), 1);
  Serial.print(" | light_level=");
  const char* lightLevel = sensors.getLightLevel();
  Serial.print(lightLevel ? lightLevel : "unavailable");
  Serial.print(" | temperature_ready=");
  Serial.print(sensors.isTemperatureReady() ? "yes" : "no");
  Serial.print(" | humidity_ready=");
  Serial.print(sensors.isHumidityReady() ? "yes" : "no");
  Serial.print(" | light_ready=");
  Serial.println(sensors.isLightReady() ? "yes" : "no");
}

void handleMenuAction(ButtonAction action) {
  if (action == ButtonAction::None) return;
  // Capture the action time before JSON serialization or network operations.
  const uint32_t epoch = timeReady ? static_cast<uint32_t>(rtc.getEpoch()) : 0;
  const ActiveEvent previous = events.getActiveEvent();
  EventManager::Result result;
  const char* title = "";

  if (action == ButtonAction::Sleep || action == ButtonAction::Walk) {
    const ActiveEvent requested = action == ButtonAction::Sleep
      ? ActiveEvent::Sleep : ActiveEvent::Walk;
    result = events.toggleDuration(requested, epoch);
    const bool stopping = previous == requested;
    title = requested == ActiveEvent::Sleep
      ? (stopping ? "Sleep stopped" : "Sleep started")
      : (stopping ? "Walk stopped" : "Walk started");
  } else if (action == ButtonAction::Feeding) {
    result = events.recordInstant(InstantEvent::Eat, epoch);
    title = "Feeding done";
  } else if (action == ButtonAction::Diaper) {
    result = events.recordInstant(InstantEvent::Diaper, epoch);
    title = "Diaper changed";
  } else {
    return;
  }

  const char* detail = "Queued for MQTT";
  if (result == EventManager::Result::TimeNotReady) {
    title = "Time not ready";
    detail = "Wait for UTC, then retry";
  } else if (result == EventManager::Result::WaitNextSecond) {
    title = "Please try again";
    detail = "One new event per second";
  } else if (result == EventManager::Result::QueueFull) {
    title = "Event not saved";
    detail = "Queue full; restore connection";
  }

  menu.showConfirmation(title, detail);
  Serial.print("[UI] ");
  Serial.println(title);
}

// Publishing happens only from loop(), never inside WiFi's local-task callback.
bool publishDiaryEvent(bool duration, const char* payload) {
  return duration ? network.publishDurationEvent(payload)
                  : network.publishInstantEvent(payload);
}

void setup() {
  Serial.begin(9600);

  unsigned long started = millis();
  while (!Serial && millis() - started < 3000) {}
  Serial.println("BabyMonitor: MQTT diary events v6");
  Serial.println(MEASUREMENTS_ENABLED
    ? "[MODE] Sensors and telemetry enabled"
    : "[MODE] Events only: sensors and telemetry disabled");
  carrier.withCase();

  if (!carrier.begin()) {
    Serial.println("Carrier kunne ikke starte");
    while (true) {
      delay(100);
    }
  }

  Serial.print("Carrier revision: ");
  Serial.println(carrier.getBoardRevision());

 

  buttons.begin();
  menu.begin();
  if (MEASUREMENTS_ENABLED) sensors.begin();
  rtc.begin();
  network.begin(serviceLocalTasks);
}


void serviceLocalTasks() {
  buttons.update();
  handleMenuAction(buttons.takeAction());
  if (MEASUREMENTS_ENABLED && sensors.update()) {
    sampleEpoch = timeReady
      ? static_cast<uint32_t>(rtc.getEpoch())
      : 0;
  }
  const unsigned long now = millis();
  reportSensorStatus(now);
  menu.update(buttons, sensors, events.getActiveEvent());
}

void syncClock() {
  const unsigned long now = millis();
  const unsigned long interval =
    timeReady ? 3600000UL : 5000UL;

  if (clockAttempted &&
      now - lastClockAttempt < interval) {
    return;
  }

  clockAttempted = true;
  lastClockAttempt = now;

  const unsigned long epoch = network.getNetworkTime();
  if (epoch < 1704067200UL) {
    Serial.println("[TIME] Waiting for NTP");
    return;
  }

  rtc.setEpoch(epoch);
  timeReady = true;

  Serial.println("[TIME] UTC clock synchronized");
}

bool formatTimestamp(
  uint32_t epoch,
  char* output,
  size_t outputSize
) {
  const time_t rawTime = static_cast<time_t>(epoch);
  struct tm utcTime = {};

  if (gmtime_r(&rawTime, &utcTime) == nullptr) {
    return false;
  }

  return strftime(
    output,
    outputSize,
    "%Y-%m-%dT%H:%M:%SZ",
    &utcTime
  ) > 0;
}

void sendTelemetryIfDue() {
  if (!MEASUREMENTS_ENABLED) return;
  const unsigned long now = millis();

  if (now - lastTelemetry < TELEMETRY_INTERVAL_MS) {
    return;
  }

  if (!network.isConnected()) {
    return;
  }

  const char* lightLevel = sensors.getLightLevel();
  if (!timeReady || sampleEpoch == 0 ||
      !sensors.isEnvironmentReady() || !lightLevel) {
    if (now - lastTelemetryDiagnostic >= 5000UL) {
      lastTelemetryDiagnostic = now;
      Serial.print("[TELEMETRY] Waiting: UTC=");
      Serial.print(timeReady && sampleEpoch != 0 ? "ready" : "missing");
      Serial.print(" | temperature=");
      Serial.print(sensors.isTemperatureReady() ? "ready" : "missing/invalid");
      Serial.print(" | humidity=");
      Serial.print(sensors.isHumidityReady() ? "ready" : "missing/invalid");
      Serial.print(" | light=");
      Serial.println(lightLevel ? "ready" : "missing");
    }
    return;
  }

  char timestamp[21];

  if (!formatTimestamp(
        sampleEpoch,
        timestamp,
        sizeof(timestamp))) {
    return;
  }

  JsonDocument document;

  document["temperature_c"] =
    round(static_cast<double>(sensors.getTemperature()) * 10.0) / 10.0;

  document["humidity_pct"] =
    round(static_cast<double>(sensors.getHumidity()) * 10.0) / 10.0;

  document["light_level"] = lightLevel;
  document["timestamp"] = timestamp;

  if (document.overflowed()) {
    return;
  }

  char payload[256];

  const size_t requiredSize = measureJson(document);

  if (requiredSize >= sizeof(payload)) {
    return;
  }

  const size_t written = serializeJson(
    document,
    payload,
    sizeof(payload)
  );

  if (written != requiredSize) {
    return;
  }

  // Limit publication attempts to one every 20 seconds.
  lastTelemetry = now;

  if (network.publishTelemetry(payload)) {
    Serial.print("[TELEMETRY] Sent: ");
    Serial.println(payload);
  } else {
    Serial.println("[TELEMETRY] Send failed");
  }
}

void loop() {
  serviceLocalTasks();
  network.update();

  syncClock();
  if (network.isConnected()) events.update(publishDiaryEvent);
  sendTelemetryIfDue();
}
