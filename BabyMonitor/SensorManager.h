#pragma once

#include <Arduino.h>
#include <Arduino_MKRIoTCarrier.h>
#include <math.h>

class SensorManager {
public:
  explicit SensorManager(
    MKRIoTCarrier& carrier,
    float temperatureOffsetC = 0.0f
  );

  void begin();
  // True when a new temperature/humidity sample has been read.
  bool update();

  float getTemperature() const;
  float getHumidity() const;
  int getBrightness() const;
  // Null when no fresh light measurement is available.
  const char* getLightLevel() const;

  bool isTemperatureReady() const;
  bool isHumidityReady() const;
  bool isEnvironmentReady() const;
  bool isLightReady() const;

private:
  void updateLightLevel(int rawValue);
  bool isEnvironmentFresh() const;

  MKRIoTCarrier& carrier;
  float temperatureOffsetC;

  static constexpr unsigned long SAMPLE_INTERVAL_MS = 500UL;
  static constexpr unsigned long DATA_TIMEOUT_MS = 2000UL;
  static constexpr unsigned long WARMUP_MS = 5000UL;

  bool started = false;
  unsigned long startedAt = 0;
  unsigned long lastSample = 0;
  unsigned long lastLight = 0;

  float temperature = NAN;
  float humidity = NAN;
  bool temperatureValid = false;
  bool humidityValid = false;

  int brightness = 0;
  bool lightValid = false;
  bool lightLevelInitialized = false;
  uint8_t lightLevel = 0;
};
