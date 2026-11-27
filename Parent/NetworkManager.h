#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

class NetworkManager {
public:
  NetworkManager();

  typedef void (*MessageCallback)(const char* topic, const char* payload, size_t size);
  typedef void (*ConnectionCallback)(bool connected);
  void begin(void (*localTasksCallback)(), MessageCallback messageCallback,
             ConnectionCallback connectionCallback);
  void update();
  bool isConnected();
  unsigned long getNetworkTime();

private:
  bool subscribeTopics();
  void connectWiFi(unsigned long now);
  void connectMqtt(unsigned long now);
  void serviceNetwork();
  void reportNetworkStatus();
  static NetworkManager* instance;
  static void onMessage(int size);
  void receiveMessage(int size);
  MessageCallback messageCallback = nullptr;
  ConnectionCallback connectionCallback = nullptr;
  bool mqttWasConnected = false;

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
