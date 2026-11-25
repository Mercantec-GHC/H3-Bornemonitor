#include <Arduino_MKRIoTCarrier.h>
#include "NetworkManager.h"
#include <RTCZero.h>

MKRIoTCarrier carrier;
NetworkManager network;
RTCZero rtc;

bool timeReady = false;
bool clockAttempted = false;
unsigned long lastClockAttempt = 0;

bool connectionViewDrawn = false;
bool lastConnected = false;
bool everConnected = false;

void serviceLocalTasks();

void showConnectionStatus() {
  const bool connected = network.isConnected();
  if (connectionViewDrawn && connected == lastConnected) return;
  if (connected) everConnected = true;

  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(2);
  carrier.display.setCursor(48, 45);
  carrier.display.print("PARENT UNIT");
  carrier.display.setCursor(30, 95);
  carrier.display.print(connected ? "CONNECTED"
    : everConnected ? "CONNECTION LOST" : "CONNECTING...");
  carrier.display.setTextSize(1);
  carrier.display.setCursor(45, 135);
  carrier.display.print(connected ? "Waiting for data"
    : everConnected ? "Reconnecting..." : "Connecting to network...");

  lastConnected = connected;
  connectionViewDrawn = true;
}

void serviceLocalTasks() {
  // Local input can still be serviced during WiFi's connection wait.
  // Do not call network APIs from this callback.
  carrier.Buttons.update();
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
  Serial.println("Parent monitor: network base");
  carrier.withCase();

  if (!carrier.begin()) {
    Serial.println("Carrier could not start");
    while (true) delay(100);
  }

  Serial.print("Carrier revision: ");
  Serial.println(carrier.getBoardRevision());
  carrier.display.setRotation(0);
  carrier.display.setFont(nullptr);
  carrier.display.setTextWrap(false);
  carrier.display.enableDisplay(true);
  if (carrier.getBoardRevision() == BOARD_REVISION_2) {
    digitalWrite(TFT_BACKLIGHT, HIGH);
  }

  rtc.begin();
  network.begin(serviceLocalTasks);
  showConnectionStatus();
}

void loop() {
  serviceLocalTasks();
  network.update();
  syncClock();
  showConnectionStatus();
}
