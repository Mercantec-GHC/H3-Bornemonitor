#pragma once
#include <Arduino_MKRIoTCarrier.h>

enum class ButtonAction { None, Sleep, Walk, Feeding, Diaper };

class ButtonManager {
public:
  explicit ButtonManager(MKRIoTCarrier& carrier);
  void begin();
  void update();
  bool menuHeld() const;
  bool measurementsHeld() const;
  ButtonAction takeAction();

private:
  MKRIoTCarrier& carrier;
  uint8_t raw = 0;
  uint8_t stable = 0;
  unsigned long changedAt = 0;
  bool armed = false;
  ButtonAction pending = ButtonAction::None;
};
