#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

class NetworkManager {
public:
  NetworkManager();

  void begin(void (*localTasksCallback)());
  void update();
  bool isConnected();
  unsigned long getNetworkTime();
  bool publishTelemetry(const char* payload);
  bool publishDurationEvent(const char* payload);
  bool publishInstantEvent(const char* payload);

private:
  bool publishMessage(const char* topic, const char* payload, uint8_t qos);
  void connectWiFi(unsigned long now);
  void connectMqtt(unsigned long now);
  void serviceNetwork();
  void reportNetworkStatus();

  WiFiSSLClient wifiClient;
  MqttClient mqtt;

  static constexpr unsigned long WIFI_RETRY_MS = 10000UL;
  static constexpr unsigned long MQTT_RETRY_MS = 5000UL;
  static constexpr unsigned long NETWORK_REPORT_MS = 5000UL;

  unsigned long lastWiFiAttempt = 0;
  unsigned long lastMqttAttempt = 0;
  unsigned long lastNetworkReport = 0;
  bool wifiAttempted = false;
  bool mqttAttempted = false;
  bool wifiWasConnected = false;
};
