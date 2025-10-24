#pragma once

#include <Arduino.h>
#include <DHT.h>

#include "hardware_config.h"

namespace sensing {

struct EnvironmentReadings {
  float temperatureC = NAN;
  float humidityPct = NAN;
  bool climateValid = false;

  uint16_t soilRaw = 0;
  float soilMoisturePct = NAN;
  bool soilValid = false;

  uint16_t lightRaw = 0;
  float lightPct = NAN;
  bool lightValid = false;
};

class SensorSuite {
 public:
  SensorSuite();

  void begin();
  EnvironmentReadings sample();
  const EnvironmentReadings& last() const { return lastReading_; }

  void setSoilCalibration(uint16_t dry, uint16_t wet);
  void setLightCalibration(uint16_t dark, uint16_t bright);

 private:
  float mapToPercent(uint16_t raw, uint16_t minimum, uint16_t maximum, bool invert) const;

  DHT dht_;
  bool started_ = false;

  uint16_t soilDry_ = hw::SOIL_RAW_DRY_DEFAULT;
  uint16_t soilWet_ = hw::SOIL_RAW_WET_DEFAULT;
  uint16_t lightDark_ = hw::LIGHT_RAW_DARK_DEFAULT;
  uint16_t lightBright_ = hw::LIGHT_RAW_BRIGHT_DEFAULT;

  bool soilPrimed_ = false;
  bool lightPrimed_ = false;
  float soilFiltered_ = 0.0f;
  float lightFiltered_ = 0.0f;

  EnvironmentReadings lastReading_;
};

}  // namespace sensing
