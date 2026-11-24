#include "NetworkManager.h"
#include "secrets.h"
#include <string.h>

namespace {
const char MQTT_CLIENT_ID[] = "babymonitor-child-01";
}

NetworkManager::NetworkManager() : mqtt(wifiClient) {}

void NetworkManager::begin(void (*localTasksCallback)()) {
  // Keep local input/sensors working during WiFiNINA's connection wait.
  WiFi.setTimeout(1000UL);
  WiFi.setFeedWatchdogFunc(localTasksCallback);

  // WiFiSSLClient provides the TLS transport required by port 8883.
  mqtt.setId(MQTT_CLIENT_ID);
  mqtt.setUsernamePassword(MQTT_USERNAME, MQTT_PASSWORD);
  mqtt.setConnectionTimeout(1500UL);
  mqtt.setKeepAliveInterval(30000UL);
}

void NetworkManager::update() {
  serviceNetwork();
  reportNetworkStatus();
}

void NetworkManager::connectMqtt(unsigned long now) {
  if (mqtt.connected()) return;
  if (mqttAttempted && now - lastMqttAttempt < MQTT_RETRY_MS) return;

  mqttAttempted = true;
  lastMqttAttempt = now;
  Serial.println("[MQTT] Connecting to broker using TLS...");
  Serial.flush();

  if (mqtt.connect(MQTT_SERVER, MQTT_PORT)) {
    Serial.println("[MQTT] Broker connected");
  } else {
    Serial.print("[MQTT] Connection failed, error: ");
    Serial.println(mqtt.connectError());
  }
}

void NetworkManager::connectWiFi(unsigned long now) {
  if (wifiAttempted && now - lastWiFiAttempt < WIFI_RETRY_MS) return;

  wifiAttempted = true;
  lastWiFiAttempt = now;
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void NetworkManager::serviceNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiWasConnected) {
      Serial.println("Wi-Fi disconnected");
      mqtt.stop();
      wifiWasConnected = false;
    }

    connectWiFi(millis());
    return;
  }

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    mqttAttempted = false;
    Serial.println("Wi-Fi connected");
    Serial.println(WiFi.localIP());
  }

  connectMqtt(millis());
  if (mqtt.connected()) mqtt.poll();
}

void NetworkManager::reportNetworkStatus() {
  unsigned long now = millis();
  if (now - lastNetworkReport < NETWORK_REPORT_MS) return;
  lastNetworkReport = now;

  Serial.print("[STATUS] Wi-Fi: ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  Serial.print(" | MQTT: ");
  Serial.println(mqtt.connected() ? "connected" : "disconnected");
}
 
bool NetworkManager::isConnected() {
  return mqtt.connected();
}

unsigned long NetworkManager::getNetworkTime() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  return WiFi.getTime();
}

bool NetworkManager::publishTelemetry(const char* payload) {
  return publishMessage(MQTT_TOPIC_TELEMETRY, payload, 0);
}

bool NetworkManager::publishDurationEvent(const char* payload) {
  return publishMessage("child/event/duration", payload, 1);
}

bool NetworkManager::publishInstantEvent(const char* payload) {
  return publishMessage("child/event/instant", payload, 1);
}

bool NetworkManager::publishMessage(const char* topic, const char* payload,
                                    uint8_t qos) {
  if (!mqtt.connected()) {
    return false;
  }

  const unsigned long length = strlen(payload);

  if (!mqtt.beginMessage(
        topic,
        length,
        false,
        qos)) {
    return false;
  }

  const size_t written = mqtt.write(
    reinterpret_cast<const uint8_t*>(payload),
    length
  );

  if (written != length) {
    mqtt.stop();
    return false;
  }

  const bool sent = mqtt.endMessage() == 1;

  if (!sent) {
    mqtt.stop();
  }

  return sent;
}
