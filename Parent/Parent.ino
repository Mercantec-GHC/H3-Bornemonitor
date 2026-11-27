#include <Arduino_MKRIoTCarrier.h>
#include "NetworkManager.h"
#include "ParentState.h"
#include "MessageHandler.h"
#include "DisplayManager.h"
#include <RTCZero.h>

MKRIoTCarrier carrier;
NetworkManager network;
RTCZero rtc;
ParentState state;
MessageHandler messages(state);
DisplayManager display(carrier);

bool timeReady = false;
bool clockAttempted = false;
unsigned long lastClockAttempt = 0;
bool rawTouch0 = false;
bool touch0Held = false;
unsigned long touchChangedAt = 0;

void serviceLocalTasks() {
  // This callback also runs during network waits; use only local hardware here.
  carrier.Buttons.update();
  const bool current = carrier.Buttons.getTouch(TOUCH0);
  const unsigned long now = millis();
  if (current != rawTouch0) {
    rawTouch0 = current;
    touchChangedAt = now;
  }
  if (now - touchChangedAt >= 35UL) touch0Held = rawTouch0;
}

void handleMessage(const char* topic, const char* payload, size_t size) {
  const MessageHandler::Result result = messages.handle(topic, payload, size,
      static_cast<uint32_t>(millis()));
  if (result == MessageHandler::Result::Updated) Serial.println("[DATA] State updated");
  else if (result == MessageHandler::Result::Invalid) Serial.println("[DATA] Invalid message ignored");
}

void handleConnectionChange(bool connected) {
  if (!connected) messages.connectionLost();
}

void syncClock() {
  const unsigned long now = millis();
  const unsigned long interval = timeReady ? 3600000UL : 5000UL;
  if (clockAttempted && now - lastClockAttempt < interval) return;
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

void setup() {
  Serial.begin(9600);
  const unsigned long started = millis();
  while (!Serial && millis() - started < 3000UL) {}
  Serial.println("Parent monitor: home screen");
  carrier.withCase();
  if (!carrier.begin()) {
    Serial.println("Carrier could not start");
    while (true) delay(100);
  }
  rtc.begin();
  display.begin();
  network.begin(serviceLocalTasks, handleMessage, handleConnectionChange);
  display.update(state, false, false, false, 0);
}

void loop() {
  serviceLocalTasks();
  network.update();
  syncClock();
  display.update(state, network.isConnected(), touch0Held, timeReady,
      timeReady ? static_cast<uint32_t>(rtc.getEpoch()) : 0);
}
