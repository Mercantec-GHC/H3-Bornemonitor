#include "DisplayManager.h"
#include "TimeUtils.h"
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t TELEMETRY_STALE_MS = 60000UL;
constexpr uint16_t SECONDARY = 0xBDF7;

void oneDecimal(float value, char* output, size_t size) {
  if (value < -999.9f || value > 999.9f) {
    snprintf(output, size, "--");
    return;
  }
  const long scaled = static_cast<long>(value * 10.0f + (value < 0 ? -0.5f : 0.5f));
  const unsigned long magnitude = scaled < 0 ? -scaled : scaled;
  snprintf(output, size, "%s%lu.%lu", scaled < 0 ? "-" : "", magnitude / 10UL, magnitude % 10UL);
}
}

DisplayManager::DisplayManager(MKRIoTCarrier& carrier) : carrier(carrier) {}

void DisplayManager::begin() {
  carrier.display.setRotation(0);
  carrier.display.setFont(nullptr);
  carrier.display.setTextWrap(false);
  carrier.display.enableDisplay(true);
  if (carrier.getBoardRevision() == BOARD_REVISION_2) digitalWrite(TFT_BACKLIGHT, HIGH);
  drawn = false;
}

void DisplayManager::centered(const char* text, int y, uint8_t size, uint16_t color) {
  while (size > 1 && strlen(text) * 6 * size > 192) --size;
  carrier.display.setTextSize(size);
  carrier.display.setTextColor(color);
  carrier.display.setCursor((240 - static_cast<int>(strlen(text) * 6 * size)) / 2, y);
  carrier.display.print(text);
}

void DisplayManager::drawDuration(const ParentState& state, bool clockReady, uint32_t epoch) {
  char duration[20];
  if (clockReady && epoch >= state.startedAt) {
    TimeUtils::formatDuration(epoch - state.startedAt, duration, sizeof(duration));
  } else {
    strcpy(duration, "--:--:--");
  }
  carrier.display.fillRect(18, 96, 204, 32, ST77XX_BLACK);
  centered(duration, 100, 3);
}

void DisplayManager::drawTelemetry(const ParentState& state, bool stale,
   bool clockReady, uint32_t epoch) {
  char text[40];
  char value[16];
  centered("ROOM", 26, 2);
  if (state.telemetry.available) {
    oneDecimal(state.telemetry.temperatureC, value, sizeof(value));
    snprintf(text, sizeof(text), "Temp: %s C", value);
    centered(text, 67, 2);
    oneDecimal(state.telemetry.humidityPct, value, sizeof(value));
    snprintf(text, sizeof(text), "Humidity: %s%%", value);
    centered(text, 108, 2);
    snprintf(text, sizeof(text), "Light: %s", state.telemetry.lightLevel);
    centered(text, 149, 2);
    uint32_t age = static_cast<uint32_t>(millis() - state.telemetry.receivedAt) / 1000UL;
    if (clockReady && epoch >= state.telemetry.sampleEpoch && epoch - state.telemetry.sampleEpoch > age) {
      age = epoch - state.telemetry.sampleEpoch;
    }
    if (stale) strcpy(text, "STALE DATA");
    else snprintf(text, sizeof(text), "Updated: %lus ago", static_cast<unsigned long>(age));
  } else {
    centered("Temp: --", 67, 2);
    centered("Humidity: --", 108, 2);
    centered("Light: --", 149, 2);
    strcpy(text, "No measurements yet");
  }
  centered(text, 183, 1, stale ? ST77XX_YELLOW : SECONDARY);
  centered("RELEASE TO RETURN", 205, 1, SECONDARY);
}

void DisplayManager::draw(const ParentState& state, View view, bool stale, bool clockReady, uint32_t epoch) {
  carrier.display.fillScreen(ST77XX_BLACK);
  if (view == View::Connecting || view == View::Lost) {
    centered("PARENT UNIT", 28, 1, SECONDARY);
    if (view == View::Lost) {
      centered("CONNECTION", 76, 2);
      centered("LOST", 108, 3);
      centered("Reconnecting...", 155, 1, SECONDARY);
    } else {
      centered("CONNECTING", 90, 2);
      centered("Please wait", 140, 1, SECONDARY);
    }
    return;
  }
  if (view == View::Offline) {
    centered("CHILD", 70, 3, ST77XX_RED);
    centered("OFFLINE", 106, 3, ST77XX_RED);
    char text[40] = "Last seen: unknown";
    if (state.lastSeen) {
      char time[6];
      TimeUtils::formatUtcTime(state.lastSeen, time, sizeof(time));
      snprintf(text, sizeof(text), "Last seen: %s UTC", time);
    }
    centered(text, 153, 1, SECONDARY);
    return;
  }
  if (view == View::Telemetry) {
    drawTelemetry(state, stale, clockReady, epoch);
    return;
  }
  centered(state.childStatus == ChildStatus::Online ? "CHILD ONLINE" : "CHILD UNKNOWN",
           27, 1, state.childStatus == ChildStatus::Online ? ST77XX_GREEN : SECONDARY);
  if (view == View::Waiting) {
    centered("WAITING", 90, 3);
    centered("Waiting for child status", 140, 1, SECONDARY);
  } else if (view == View::Active) {
    centered("ACTIVE", 90, 3);
    centered("No event recorded", 140, 1, SECONDARY);
  } else {
    centered(state.activity == Activity::Sleep ? "SLEEP" : "WALK", 61, 3);
    drawDuration(state, clockReady, epoch);
    char time[6];
    char text[32];
    TimeUtils::formatUtcTime(state.startedAt, time, sizeof(time));
    snprintf(text, sizeof(text), "Started: %s UTC", time);
    centered(text, 148, 1, SECONDARY);
    if (state.activityMayBeStale) centered("Last known event", 173, 1, ST77XX_YELLOW);
    else if (!clockReady || epoch < state.startedAt) centered("Waiting for UTC clock", 173, 1, SECONDARY);
  }
  centered("HOLD 0: MEASUREMENTS", 205, 1, SECONDARY);
}

void DisplayManager::update(const ParentState& state, bool connected, bool touch0Held, bool clockReady, uint32_t epoch) {
  if (connected) everConnected = true;
  const View view = !connected ? (everConnected ? View::Lost : View::Connecting)
    : state.childStatus == ChildStatus::Offline ? View::Offline
    : touch0Held ? View::Telemetry
    : state.childStatus == ChildStatus::Unknown ? View::Waiting
    : state.activity == Activity::None ? View::Active : View::Duration;
  bool stale = state.telemetry.available &&
    static_cast<uint32_t>(millis() - state.telemetry.receivedAt) >= TELEMETRY_STALE_MS;
  if (state.telemetry.available && clockReady && epoch >= state.telemetry.sampleEpoch &&
      epoch - state.telemetry.sampleEpoch >= TELEMETRY_STALE_MS / 1000UL) stale = true;
  const uint32_t second = clockReady ? epoch : static_cast<uint32_t>(millis()) / 1000UL;
  const bool usableClock = clockReady && epoch >= state.startedAt;
  const bool changedContent = view == View::Telemetry ? state.revision != lastRevision
    : view == View::Duration ? state.eventId != lastEventId || state.activity != lastActivity ||
      state.startedAt != lastStartedAt || state.activityMayBeStale != lastActivityStale ||
      usableClock != lastUsableClock
    : view == View::Offline ? state.lastSeen != lastSeen : false;
  const bool redraw = !drawn || view != lastView || changedContent ||
    (view == View::Telemetry && stale != lastStale) || clockReady != lastClockReady;
  if (redraw || (view == View::Telemetry && second != lastSecond)) {
    draw(state, view, stale, clockReady, epoch);
  } else if (view == View::Duration && second != lastSecond) {
    drawDuration(state, clockReady, epoch);
  }
  drawn = true;
  lastView = view;
  lastRevision = state.revision;
  lastSecond = second;
  lastStale = stale;
  lastClockReady = clockReady;
  lastEventId = state.eventId;
  lastActivity = state.activity;
  lastStartedAt = state.startedAt;
  lastActivityStale = state.activityMayBeStale;
  lastUsableClock = usableClock;
  lastSeen = state.lastSeen;
}
