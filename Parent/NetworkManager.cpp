#include "NetworkManager.h"
#include "secrets.h"

namespace {
const char MQTT_CLIENT_ID[] = "babymonitor-parent-01";
}

NetworkManager* NetworkManager::instance = nullptr;

NetworkManager::NetworkManager() : mqtt(wifiClient) {}

void NetworkManager::begin(void (*localTasksCallback)(), MessageCallback incoming,
                            ConnectionCallback connection) {
  instance = this;
  messageCallback = incoming;
  connectionCallback = connection;
  mqtt.onMessage(onMessage);
  WiFi.setTimeout(1000UL);
  WiFi.setFeedWatchdogFunc(localTasksCallback);

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

  if (!mqtt.connect(MQTT_SERVER, MQTT_PORT)) {
    Serial.print("[MQTT] Connection failed, error: ");
    Serial.println(mqtt.connectError());
    return;
  }
  Serial.println("[MQTT] Broker connected");
  mqttWasConnected = true;
  if (connectionCallback) connectionCallback(true);

  if (!subscribeTopics()) {
    Serial.println("[MQTT] Reconnecting to retry subscriptions...");
    mqtt.stop();
    mqttWasConnected = false;
    if (connectionCallback) connectionCallback(false);
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
  if (mqttWasConnected && (WiFi.status() != WL_CONNECTED || !mqtt.connected())) {
    mqttWasConnected = false;
    if (connectionCallback) connectionCallback(false);
  }
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
  if (mqtt.connected()) {
    mqtt.poll();
  }
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

bool NetworkManager::subscribeTopics() {
  const char* topics[] = {
    MQTT_TOPIC_TELEMETRY,
    MQTT_TOPIC_DURATION,
    MQTT_TOPIC_INSTANT,
    MQTT_TOPIC_ALARM
  };

  for (const char* topic : topics) {
    if (!mqtt.subscribe(topic, 1)) {
      Serial.print("[MQTT] Subscription failed: ");
      Serial.println(topic);
      return false;
    }

    Serial.print("[MQTT] Subscribed: ");
    Serial.println(topic);
  }

  return true;
}

void NetworkManager::onMessage(int size) {
  if (instance) instance->receiveMessage(size);
}

void NetworkManager::receiveMessage(int size) {
  char payload[512];
  if (size <= 0 || static_cast<size_t>(size) >= sizeof(payload)) {
    Serial.println("[RX] Unsupported message size; reconnecting...");
    mqtt.stop();
    return;
  }
  const String topic = mqtt.messageTopic();
  for (int i = 0; i < size; ++i) {
    const int value = mqtt.read();
    if (value < 0) {
      Serial.println("[RX] Incomplete message; reconnecting...");
      mqtt.stop();
      return;
    }
    payload[i] = static_cast<char>(value);
  }
  payload[size] = '\0';
  Serial.print("[RX] Topic: ");
  Serial.println(topic);
  Serial.print("[RX] JSON: ");
  Serial.write(reinterpret_cast<const uint8_t*>(payload), size);
  Serial.println();
  if (messageCallback) messageCallback(topic.c_str(), payload, size);
}
