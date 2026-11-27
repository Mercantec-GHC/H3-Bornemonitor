#include "MessageHandler.h"
#include "TimeUtils.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

MessageHandler::MessageHandler(ParentState& state) : state(state) {}

void MessageHandler::connectionLost() {
  state.childStatus = ChildStatus::Unknown;
  state.activityMayBeStale = true;
  ++state.revision;
}

MessageHandler::Result MessageHandler::handle(const char* topic, const char* payload,
                                               size_t size, uint32_t receivedAt) {
  if (!topic || !payload || size == 0 || size > 511) return Result::Invalid;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, payload, size,
      DeserializationOption::NestingLimit(4));
  if (error || !document.is<JsonObject>()) return Result::Invalid;

  if (strcmp(topic, MQTT_TOPIC_TELEMETRY) == 0) {
    if (!document["temperature_c"].is<float>() || !document["humidity_pct"].is<float>() ||
        !document["light_level"].is<const char*>() || !document["timestamp"].is<const char*>()) {
      return Result::Invalid;
    }
    const float temperature = document["temperature_c"].as<float>();
    const float humidity = document["humidity_pct"].as<float>();
    const char* level = document["light_level"].as<const char*>();
    if (!isfinite(temperature) || !isfinite(humidity) || humidity < 0 || humidity > 100) {
      return Result::Invalid;
    }
    if (strcmp(level, "very_low") && strcmp(level, "low") && strcmp(level, "medium") &&
        strcmp(level, "high") && strcmp(level, "very_high")) return Result::Invalid;
    uint32_t epoch;
    if (!TimeUtils::parseUtc(document["timestamp"], epoch)) return Result::Invalid;
    if (state.telemetry.available && epoch <= state.telemetry.sampleEpoch) return Result::Ignored;
    state.telemetry.temperatureC = temperature;
    state.telemetry.humidityPct = humidity;
    strcpy(state.telemetry.lightLevel, level);
    state.telemetry.sampleEpoch = epoch;
    state.telemetry.receivedAt = receivedAt;
    state.telemetry.available = true;
    ++state.revision;
    return Result::Updated;
  }

  if (strcmp(topic, MQTT_TOPIC_ALARM) == 0) {
    const char* value = document["state"] | "";
    if (strcmp(value, "online") != 0 && strcmp(value, "offline") != 0) return Result::Invalid;
    uint32_t lastSeen = 0;
    if (!document["last_seen"].isNull()) {
      if (!document["last_seen"].is<const char*>() ||
          !TimeUtils::parseUtc(document["last_seen"], lastSeen)) return Result::Invalid;
    }
    state.childStatus = strcmp(value, "online") == 0 ? ChildStatus::Online : ChildStatus::Offline;
    if (lastSeen) state.lastSeen = lastSeen;
    ++state.revision;
    return Result::Updated;
  }

  const bool isDuration = strcmp(topic, MQTT_TOPIC_DURATION) == 0;
  const bool isInstant = strcmp(topic, MQTT_TOPIC_INSTANT) == 0;
  if (!isDuration && !isInstant) return Result::Ignored;
  if (!document["event_id"].is<uint32_t>() || !document["type"].is<const char*>() ||
      !document["timestamp"].is<const char*>()) return Result::Invalid;
  const uint32_t id = document["event_id"].as<uint32_t>();
  uint32_t epoch;
  if (id == 0 || !TimeUtils::parseUtc(document["timestamp"], epoch)) return Result::Invalid;
  const char* type = document["type"];

  if (isInstant) {
    if (strcmp(type, "eat") && strcmp(type, "diaper")) return Result::Invalid;
    if (state.instantAvailable && (epoch < state.instantEpoch ||
        (epoch == state.instantEpoch && id <= state.instantId))) return Result::Ignored;
    state.instantAvailable = true;
    state.instantType = strcmp(type, "eat") == 0 ? InstantType::Eat : InstantType::Diaper;
    state.instantId = id;
    state.instantEpoch = epoch;
    return Result::Updated;
  }

  const Activity activity = strcmp(type, "sleep") == 0 ? Activity::Sleep
    : strcmp(type, "walk") == 0 ? Activity::Walk : Activity::None;
  const char* action = document["action"] | "";
  if (activity == Activity::None || (strcmp(action, "start") && strcmp(action, "stop"))) {
    return Result::Invalid;
  }
  const bool stopped = strcmp(action, "stop") == 0;
  if (epoch < durationEpoch || (epoch == durationEpoch && (id < durationId ||
      (id == durationId && (durationStopped || !stopped))))) return Result::Ignored;
  if (!stopped && state.activity != Activity::None && id == state.eventId) {
    return activity == state.activity && epoch == state.startedAt ? Result::Ignored : Result::Invalid;
  }
  if (stopped && id == state.eventId && state.activity != Activity::None && activity != state.activity) {
    return Result::Invalid;
  }
  durationEpoch = epoch;
  durationId = id;
  durationStopped = stopped;
  if (!stopped) {
    state.activity = activity;
    state.eventId = id;
    state.startedAt = epoch;
    state.activityMayBeStale = false;
  } else if (id == state.eventId && activity == state.activity) {
    state.activity = Activity::None;
    state.eventId = 0;
    state.startedAt = 0;
    state.activityMayBeStale = false;
  }
  ++state.revision;
  return Result::Updated;
}
