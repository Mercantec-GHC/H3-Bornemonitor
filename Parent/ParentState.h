#pragma once
#include <stdint.h>

enum class ChildStatus { Unknown, Online, Offline };
enum class Activity { None, Sleep, Walk };
enum class InstantType { Eat, Diaper };

struct TelemetryData {
  bool available = false;
  float temperatureC = 0;
  float humidityPct = 0;
  char lightLevel[10] = {};
  uint32_t sampleEpoch = 0;
  uint32_t receivedAt = 0;
};

struct ParentState {
  ChildStatus childStatus = ChildStatus::Unknown;
  uint32_t lastSeen = 0;
  TelemetryData telemetry;
  Activity activity = Activity::None;
  uint32_t eventId = 0;
  uint32_t startedAt = 0;
  bool activityMayBeStale = false;
  bool instantAvailable = false;
  InstantType instantType = InstantType::Eat;
  uint32_t instantId = 0;
  uint32_t instantEpoch = 0;
  uint32_t revision = 0;
};
