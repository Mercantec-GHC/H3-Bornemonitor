#pragma once
#include "ParentState.h"
#include <stddef.h>

class MessageHandler {
public:
  enum class Result { Updated, Ignored, Invalid };
  explicit MessageHandler(ParentState& state);
  Result handle(const char* topic, const char* payload, size_t size, uint32_t receivedAt);
  void connectionLost();

private:
  ParentState& state;
  uint32_t durationEpoch = 0;
  uint32_t durationId = 0;
  bool durationStopped = false;
};
