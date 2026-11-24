#include "SensorManager.h"

namespace {
// Starting thresholds in raw sensor units; calibrate with the case fitted.
constexpr int LIGHT_THRESHOLDS[] = {20, 50, 200, 500};
constexpr int LIGHT_HYSTERESIS = 5;
const char* const LIGHT_LEVEL_NAMES[] = {
  "very_low", "low", "medium", "high", "very_high"
};
}

SensorManager::SensorManager(
  MKRIoTCarrier& carrier,
  float temperatureOffsetC
) : carrier(carrier), temperatureOffsetC(temperatureOffsetC) {}

void SensorManager::begin() {
  startedAt = millis();
  lastSample = startedAt;
  lastLight = startedAt;
  temperature = NAN;
  humidity = NAN;
  temperatureValid = false;
  humidityValid = false;
  brightness = 0;
  lightValid = false;
  lightLevelInitialized = false;
  lightLevel = 0;
  started = true;
}

bool SensorManager::update() {
  if (!started) return false;

  const unsigned long now = millis();

  if (lightValid && now - lastLight > DATA_TIMEOUT_MS) {
    lightValid = false;
    lightLevelInitialized = false;
  }

  if (carrier.Light.colorAvailable()) {
    int r, g, b;
    if (carrier.Light.readColor(r, g, b) && r >= 0 && g >= 0 && b >= 0) {
      brightness = (r + g + b) / 3;
      updateLightLevel(brightness);
      lightValid = true;
      lastLight = millis();
    }
  }

  if (now - lastSample < SAMPLE_INTERVAL_MS) return false;

  temperature = carrier.Env.readTemperature() + temperatureOffsetC;
  humidity = carrier.Env.readHumidity();
  lastSample = millis();

  temperatureValid = isfinite(temperature);
  humidityValid = isfinite(humidity) && humidity >= 0 && humidity <= 100;
  return true;
}

float SensorManager::getTemperature() const {
  return temperature;
}

float SensorManager::getHumidity() const {
  return humidity;
}

int SensorManager::getBrightness() const {
  return brightness;
}

const char* SensorManager::getLightLevel() const {
  return isLightReady() ? LIGHT_LEVEL_NAMES[lightLevel] : nullptr;
}

bool SensorManager::isEnvironmentFresh() const {
  const unsigned long now = millis();
  return started && now - startedAt >= WARMUP_MS &&
    now - lastSample <= DATA_TIMEOUT_MS;
}

bool SensorManager::isTemperatureReady() const {
  return temperatureValid && isEnvironmentFresh();
}

bool SensorManager::isHumidityReady() const {
  return humidityValid && isEnvironmentFresh();
}

bool SensorManager::isEnvironmentReady() const {
  return isTemperatureReady() && isHumidityReady();
}

bool SensorManager::isLightReady() const {
  return started && lightValid && millis() - lastLight <= DATA_TIMEOUT_MS;
}

void SensorManager::updateLightLevel(int rawValue) {
  if (!lightLevelInitialized) {
    lightLevel = 0;
    while (lightLevel < 4 && rawValue >= LIGHT_THRESHOLDS[lightLevel]) {
      ++lightLevel;
    }
    lightLevelInitialized = true;
    return;
  }

  // Separate rising/falling thresholds keep noisy values from flickering.
  while (lightLevel < 4 &&
         rawValue >= LIGHT_THRESHOLDS[lightLevel] + LIGHT_HYSTERESIS) {
    ++lightLevel;
  }
  while (lightLevel > 0 &&
         rawValue < LIGHT_THRESHOLDS[lightLevel - 1] - LIGHT_HYSTERESIS) {
    --lightLevel;
  }
}
