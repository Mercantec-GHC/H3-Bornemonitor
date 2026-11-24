#pragma once
#include "ButtonManager.h"
#include "SensorManager.h"

#include "EventManager.h"

class MenuManager {
public:
  explicit MenuManager(MKRIoTCarrier& carrier);
  void begin();
  void showConfirmation(const char* title, const char* detail);
  void update(const ButtonManager& buttons, const SensorManager& sensors, ActiveEvent active);

private:
  enum class View { Off, Menu, Sensors, Confirmation };
  void setEnabled(bool enabled);
  void centered(const char* text, int y, uint8_t size, uint16_t color);
  void drawMenu(ActiveEvent active);
  void drawSensors(const SensorManager& sensors);
  void drawConfirmation();

  MKRIoTCarrier& carrier;
  View lastView = View::Off;
  ActiveEvent lastActive = ActiveEvent::None;
  bool screenOn = true;
  bool redraw = true;
  bool hasMessage = false;
  unsigned long lastDraw = 0;
  unsigned long messageStarted = 0;
  char messageTitle[24] = {};
  char messageDetail[32] = {};
};
