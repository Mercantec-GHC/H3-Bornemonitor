#include "ButtonManager.h"

ButtonManager::ButtonManager(MKRIoTCarrier& carrier) : carrier(carrier) {}

void ButtonManager::begin() {
  raw = stable = 0;
  changedAt = millis();
  armed = false;
  pending = ButtonAction::None;
}

void ButtonManager::update() {
  carrier.Buttons.update();
  uint8_t current = 0;
  if (carrier.Buttons.getTouch(TOUCH0)) current |= 1;
  if (carrier.Buttons.getTouch(TOUCH1)) current |= 2;
  if (carrier.Buttons.getTouch(TOUCH2)) current |= 4;
  if (carrier.Buttons.getTouch(TOUCH3)) current |= 8;
  if (carrier.Buttons.getTouch(TOUCH4)) current |= 16;

  const unsigned long now = millis();
  if (current != raw) {
    raw = current;
    changedAt = now;
  }
  if (raw == stable || now - changedAt < 35UL) return;

  const uint8_t previous = stable;
  stable = raw;
  if (!menuHeld()) {
    armed = false;
    return;
  }

  const uint8_t actions = stable & 30;
  if (!(previous & 1)) {
    armed = actions == 0;
    return;
  }
  if (actions == 0) {
    armed = true;
    return;
  }
  if (!armed) return;
  armed = false;

  // Accept one button only, after all event buttons were released.
  if ((actions & (actions - 1)) != 0 || (previous & 30) != 0) return;
  if (pending != ButtonAction::None) return;
  if (actions == 2) pending = ButtonAction::Sleep;
  if (actions == 4) pending = ButtonAction::Walk;
  if (actions == 8) pending = ButtonAction::Feeding;
  if (actions == 16) pending = ButtonAction::Diaper;
}

bool ButtonManager::menuHeld() const {
  return (stable & 1) != 0;
}

bool ButtonManager::measurementsHeld() const {
  return !menuHeld() && (stable & 2) != 0;
}

ButtonAction ButtonManager::takeAction() {
  const ButtonAction result = pending;
  pending = ButtonAction::None;
  return result;
}
