#include "EventManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

EventManager::Result EventManager::checkTime(uint32_t epoch, bool createsId) const {
  if (epoch < 1704067200UL || epoch < lastRecordedEpoch) {
    return Result::TimeNotReady;
  }
  
  if (createsId && epoch <= lastCreatedId) return Result::WaitNextSecond;
  return Result::Accepted;
}

void EventManager::enqueue(Type type, Action action, uint32_t id, uint32_t epoch) {
  const size_t tail = (head + count) % QUEUE_CAPACITY;
  queue[tail] = {id, epoch, type, action};
  ++count;
}

EventManager::Result EventManager::toggleDuration(ActiveEvent requested, uint32_t epoch) {
  if (requested == ActiveEvent::None) return Result::Accepted;
  const bool stopping = active == requested;
  const Result timeResult = checkTime(epoch, !stopping);
  if (timeResult != Result::Accepted) return timeResult;

  const size_t required = !stopping && active != ActiveEvent::None ? 2 : 1;
  // Reserve both records before changing state when switching sleep/walk.
  if (QUEUE_CAPACITY - count < required) return Result::QueueFull;

  if (active != ActiveEvent::None) {
    enqueue(active == ActiveEvent::Sleep ? Type::Sleep : Type::Walk,
            Action::Stop, activeId, epoch);
  }
  if (stopping) {
    active = ActiveEvent::None;
    activeId = 0;
  } else {
    enqueue(requested == ActiveEvent::Sleep ? Type::Sleep : Type::Walk,
            Action::Start, epoch, epoch);
    active = requested;
    activeId = epoch;
    lastCreatedId = epoch;
  }
  lastRecordedEpoch = epoch;
  Serial.print("[EVENT] Queued duration; pending=");
  Serial.println(static_cast<unsigned int>(count));
  return Result::Accepted;
}

EventManager::Result EventManager::recordInstant(InstantEvent type, uint32_t epoch) {
  const Result timeResult = checkTime(epoch, true);
  if (timeResult != Result::Accepted) return timeResult;
  if (count == QUEUE_CAPACITY) return Result::QueueFull;

  enqueue(type == InstantEvent::Eat ? Type::Eat : Type::Diaper,
          Action::Instant, epoch, epoch);
  lastCreatedId = epoch;
  lastRecordedEpoch = epoch;
  Serial.print("[EVENT] Queued instant; pending=");
  Serial.println(static_cast<unsigned int>(count));
  return Result::Accepted;
}

bool EventManager::buildPayload(const Message& message, char* output, size_t size) const {
  const time_t rawTime = static_cast<time_t>(message.epoch);
  struct tm utcTime = {};
  char timestamp[21];
  if (!gmtime_r(&rawTime, &utcTime) ||
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utcTime) == 0) {
    return false;
  }

  const char* type = message.type == Type::Sleep ? "sleep"
    : message.type == Type::Walk ? "walk"
    : message.type == Type::Eat ? "eat" : "diaper";
  JsonDocument document;
  document["event_id"] = message.eventId;
  document["type"] = type;
  if (message.action != Action::Instant) {
    document["action"] = message.action == Action::Start ? "start" : "stop";
  }
  document["timestamp"] = timestamp;

  if (document.overflowed()) return false;
  const size_t required = measureJson(document);
  return required < size && serializeJson(document, output, size) == required;
}

void EventManager::update(Publisher publish) {
  if (count == 0 || !publish) return;
  const uint32_t now = static_cast<uint32_t>(millis());
  if (retryPending && static_cast<uint32_t>(now - failedAt) < 1000UL) return;

  const Message message = queue[head];
  char payload[192];
  if (!buildPayload(message, payload, sizeof(payload))) {
    retryPending = true;
    failedAt = static_cast<uint32_t>(millis());
    Serial.println("[EVENT] JSON failed; message kept in queue");
    return;
  }

  const bool duration = message.action != Action::Instant;
  if (!publish(duration, payload)) {
    retryPending = true;
    failedAt = static_cast<uint32_t>(millis());
    Serial.println("[EVENT] Publish not acknowledged; message kept in queue");
    return;
  }

  head = (head + 1) % QUEUE_CAPACITY;
  --count;
  retryPending = false;
  Serial.print("[EVENT] Broker acknowledged ");
  Serial.print(duration ? "child/event/duration: " : "child/event/instant: ");
  Serial.println(payload);
}
