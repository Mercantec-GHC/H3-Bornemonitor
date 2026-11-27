#pragma once

#include <stddef.h>
#include <stdint.h>

enum class ActiveEvent { None, Sleep, Walk };
enum class InstantEvent { Eat, Diaper };

class EventManager {
public:
  enum class Result { Accepted, TimeNotReady, WaitNextSecond, QueueFull };

  typedef bool (*Publisher)(bool duration, const char* payload);

  Result toggleDuration(ActiveEvent requested, uint32_t epoch);
  Result recordInstant(InstantEvent type, uint32_t epoch);
  void update(Publisher publish);

  ActiveEvent getActiveEvent() const { return active; }
  size_t pendingCount() const { return count; }

private:
  enum class Type : uint8_t { Sleep, Walk, Eat, Diaper };
  enum class Action : uint8_t { Start, Stop, Instant };
  struct Message {
    uint32_t eventId;
    uint32_t epoch;
    Type type;
    Action action;
  };

  static constexpr size_t QUEUE_CAPACITY = 32;
  Result checkTime(uint32_t epoch, bool createsId) const;
  void enqueue(Type type, Action action, uint32_t id, uint32_t epoch);
  bool buildPayload(const Message& message, char* output, size_t size) const;

  // RAM queue: a power cycle clears pending messages and the active event.
  Message queue[QUEUE_CAPACITY] = {};
  size_t head = 0;
  size_t count = 0;
  ActiveEvent active = ActiveEvent::None;
  uint32_t activeId = 0;
  uint32_t lastCreatedId = 0;
  uint32_t lastRecordedEpoch = 0;
  bool retryPending = false;
  uint32_t failedAt = 0;
};
