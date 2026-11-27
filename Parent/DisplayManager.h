#pragma once
#include <Arduino_MKRIoTCarrier.h>
#include "ParentState.h"

class DisplayManager {
public:
  explicit DisplayManager(MKRIoTCarrier& carrier);
  void begin();
  void update(const ParentState& state, bool connected, bool touch0Held, bool clockReady, uint32_t epoch);

private:
  enum class View { Connecting, Lost, Offline, Waiting, Active, Duration, Telemetry };
  void centered(const char* text, int y, uint8_t size, uint16_t color = ST77XX_WHITE);
  void drawDuration(const ParentState& state, bool clockReady, uint32_t epoch);
  void drawTelemetry(const ParentState& state, bool stale, bool clockReady, uint32_t epoch);
  void draw(const ParentState& state, View view, bool stale, bool clockReady, uint32_t epoch);
  MKRIoTCarrier& carrier;
  bool drawn = false;
  bool everConnected = false;
  View lastView = View::Connecting;
  uint32_t lastRevision = 0;
  uint32_t lastSecond = 0;
  bool lastStale = false;
  bool lastClockReady = false;
  uint32_t lastEventId = 0;
  Activity lastActivity = Activity::None;
  uint32_t lastStartedAt = 0;
  uint32_t lastSeen = 0;
  bool lastActivityStale = false;
  bool lastUsableClock = false;
};
