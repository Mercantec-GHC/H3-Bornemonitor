#include "MenuManager.h"
#include <string.h>

MenuManager::MenuManager(MKRIoTCarrier& carrier) : carrier(carrier) {}

void MenuManager::begin() {
  carrier.display.setRotation(0);
  carrier.display.setFont(nullptr);
  carrier.display.setTextWrap(false);
  carrier.display.fillScreen(ST77XX_BLACK);
  screenOn = true;
  setEnabled(false);
  lastView = View::Off;
  redraw = true;
}

void MenuManager::setEnabled(bool enabled) {
  if (screenOn == enabled) return;
  carrier.display.enableDisplay(enabled);
  if (carrier.getBoardRevision() == BOARD_REVISION_2) {
    digitalWrite(TFT_BACKLIGHT, enabled ? HIGH : LOW);
  }
  screenOn = enabled;
}

void MenuManager::showConfirmation(const char* title, const char* detail) {
  strncpy(messageTitle, title, sizeof(messageTitle) - 1);
  messageTitle[sizeof(messageTitle) - 1] = '\0';
  strncpy(messageDetail, detail, sizeof(messageDetail) - 1);
  messageDetail[sizeof(messageDetail) - 1] = '\0';
  messageStarted = millis();
  hasMessage = true;
  redraw = true;
}

void MenuManager::centered(const char* text, int y,
                           uint8_t size, uint16_t color) {
  const int width = strlen(text) * 6 * size;
  carrier.display.setTextSize(size);
  carrier.display.setTextColor(color);
  carrier.display.setCursor((240 - width) / 2, y);
  carrier.display.print(text);
}

void MenuManager::drawMenu(ActiveEvent active) {
  const char* status = active == ActiveEvent::Sleep ? "SLEEP ACTIVE"
    : active == ActiveEvent::Walk ? "WALK ACTIVE" : "NO ACTIVE EVENT";
  centered(status, 22, 1, ST77XX_WHITE);
  centered("EVENTS", 40, 2, ST77XX_WHITE);

  const char* names[] = {"Sleep", "Walk", "Feeding", "Diaper"};
  for (uint8_t i = 0; i < 4; ++i) {
    const bool selected = (i == 0 && active == ActiveEvent::Sleep) ||
                          (i == 1 && active == ActiveEvent::Walk);
    const char* action = i >= 2 ? "LOG" : selected ? "STOP" : "START";
    const int y = 65 + i * 30;
    if (selected) {
      carrier.display.fillRoundRect(28, y, 184, 26, 4, 0xB757);
    } else {
      carrier.display.drawFastHLine(28, y + 25, 184, 0x3186);
    }
    carrier.display.setTextSize(2);
    carrier.display.setTextColor(selected ? ST77XX_BLACK : ST77XX_WHITE);
    carrier.display.setCursor(36, y + 5);
    carrier.display.print(i + 1);
    carrier.display.print(' ');
    carrier.display.print(names[i]);
    carrier.display.setCursor(206 - strlen(action) * 12, y + 5);
    carrier.display.print(action);
  }
  centered("HOLD 0 / TAP 1-4", 205, 1, ST77XX_WHITE);
}

void MenuManager::drawSensors(const SensorManager& sensors) {
  centered("ROOM", 40, 2, ST77XX_WHITE);
  carrier.display.setTextSize(2);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setCursor(30, 74);
  carrier.display.print("Temp: ");
  if (sensors.isTemperatureReady()) {
    carrier.display.print(sensors.getTemperature(), 1);
    carrier.display.print(" C");
  } else carrier.display.print("--");

  carrier.display.setCursor(30, 112);
  carrier.display.print("Humidity:");
  if (sensors.isHumidityReady()) {
    carrier.display.print(sensors.getHumidity(), 1);
    carrier.display.print('%');
  } else carrier.display.print("--");

  carrier.display.setCursor(30, 150);
  carrier.display.print("Light:");
  const char* level = sensors.getLightLevel();
  carrier.display.print(level ? level : "--");
}

void MenuManager::drawConfirmation() {
  centered("EVENT", 40, 2, ST77XX_WHITE);
  centered(messageTitle, 88, 2, ST77XX_WHITE);
  centered(messageDetail, 126, 1, ST77XX_WHITE);
}

void MenuManager::update(const ButtonManager& buttons, const SensorManager& sensors, ActiveEvent active) {
  const unsigned long now = millis();
  if (!buttons.menuHeld() || now - messageStarted >= 1400UL) hasMessage = false;
  const View view = buttons.menuHeld()
    ? (hasMessage ? View::Confirmation : View::Menu)
    : buttons.measurementsHeld() ? View::Sensors : View::Off;

  if (view == View::Off) {
    if (lastView != View::Off || redraw) {
      carrier.display.fillScreen(ST77XX_BLACK);
      setEnabled(false);
    }
    lastView = view;
    redraw = false;
    return;
  }

  const bool changed = view != lastView || active != lastActive || redraw;
  if (!changed && (view != View::Sensors || now - lastDraw < 250UL)) return;

  carrier.display.fillScreen(ST77XX_BLACK);
  if (view == View::Menu) drawMenu(active);
  if (view == View::Sensors) drawSensors(sensors);
  if (view == View::Confirmation) drawConfirmation();
  setEnabled(true);
  lastView = view;
  lastActive = active;
  lastDraw = now;
  redraw = false;
}
