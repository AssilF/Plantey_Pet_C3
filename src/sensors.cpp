#include "sensors.h"

#include <algorithm>
#include <cmath>

namespace sensing {

namespace {
float clampf(float value, float minValue, float maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}
}  // namespace

SensorSuite::SensorSuite() : dht_(hw::PIN_DHT, DHT11) {}

void SensorSuite::begin() {
#if defined(ESP32)
  analogReadResolution(12);
#endif
  dht_.begin();
  pinMode(hw::PIN_SOIL_SENSOR, INPUT);
  pinMode(hw::PIN_LDR_SENSOR, INPUT);
  started_ = true;
}

EnvironmentReadings SensorSuite::sample() {
  if (!started_) {
    begin();
  }

  EnvironmentReadings reading;

  // DHT11 climate data.
  float humidity = dht_.readHumidity();
  float temperature = dht_.readTemperature();
  bool climateValid = !(std::isnan(humidity) || std::isnan(temperature));
  if (climateValid) {
    lastReading_.humidityPct = humidity;
    lastReading_.temperatureC = temperature;
    lastReading_.climateValid = true;
  }

  reading.humidityPct = lastReading_.humidityPct;
  reading.temperatureC = lastReading_.temperatureC;
  reading.climateValid = lastReading_.climateValid;

  // Soil and light analog channels.
  uint16_t soilRaw = analogRead(hw::PIN_SOIL_SENSOR);
  uint16_t lightRaw = analogRead(hw::PIN_LDR_SENSOR);

  if (!soilPrimed_) {
    soilFiltered_ = static_cast<float>(soilRaw);
    soilPrimed_ = true;
  } else {
    soilFiltered_ = (1.0f - hw::SOIL_ALPHA) * soilFiltered_ + hw::SOIL_ALPHA * soilRaw;
  }

  if (!lightPrimed_) {
    lightFiltered_ = static_cast<float>(lightRaw);
    lightPrimed_ = true;
  } else {
    lightFiltered_ = (1.0f - hw::LIGHT_ALPHA) * lightFiltered_ + hw::LIGHT_ALPHA * lightRaw;
  }

  float soilPct = mapToPercent(static_cast<uint16_t>(soilFiltered_), soilWet_, soilDry_, true);
  float lightPct = mapToPercent(static_cast<uint16_t>(lightFiltered_), lightBright_, lightDark_, true);

  lastReading_.soilRaw = soilRaw;
  lastReading_.soilMoisturePct = soilPct;
  lastReading_.soilValid = true;

  lastReading_.lightRaw = lightRaw;
  lastReading_.lightPct = lightPct;
  lastReading_.lightValid = true;

  reading.soilRaw = soilRaw;
  reading.soilMoisturePct = soilPct;
  reading.soilValid = true;

  reading.lightRaw = lightRaw;
  reading.lightPct = lightPct;
  reading.lightValid = true;

  return reading;
}

void SensorSuite::setSoilCalibration(uint16_t dry, uint16_t wet) {
  if (dry == wet) {
    return;
  }
  soilDry_ = std::max(dry, wet);
  soilWet_ = std::min(dry, wet);
}

void SensorSuite::setLightCalibration(uint16_t dark, uint16_t bright) {
  if (dark == bright) {
    return;
  }
  lightDark_ = std::max(dark, bright);
  lightBright_ = std::min(dark, bright);
}

float SensorSuite::mapToPercent(uint16_t raw, uint16_t minimum, uint16_t maximum, bool invert) const {
  if (maximum == minimum) {
    return NAN;
  }
  float percent = (static_cast<float>(raw) - minimum) / (static_cast<float>(maximum) - minimum);
  percent = clampf(percent, 0.0f, 1.0f);
  if (invert) {
    percent = 1.0f - percent;
  }
  return percent * 100.0f;
}

}  // namespace sensing
