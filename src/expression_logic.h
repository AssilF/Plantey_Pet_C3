#pragma once

#include <Arduino.h>

#include "display_manager.h"
#include "sensors.h"

namespace brain {

enum class MoodKind : uint8_t {
  Joyful,
  Content,
  Thirsty,
  Overwatered,
  Sleepy,
  SeekingLight,
  TooBright,
  TooHot,
  TooCold,
  Curious,
};

struct MoodResult {
  MoodKind mood = MoodKind::Content;
  display::FaceExpressionView face;
  const char* tip = nullptr;
  bool playHydrationCue = false;
  bool playCelebrationCue = false;
};

class ExpressionLogic {
 public:
  MoodResult evaluate(const sensing::EnvironmentReadings& env);

  void setSoilThresholds(float dryPct, float soggyPct) {
    soilDryThreshold_ = dryPct;
    soilSoggyThreshold_ = soggyPct;
  }

  void setLightThresholds(float lowPct, float highPct) {
    lightLowThreshold_ = lowPct;
    lightHighThreshold_ = highPct;
  }

  void setTemperatureComfortRange(float minComfort, float maxComfort) {
    comfortTempMinC_ = minComfort;
    comfortTempMaxC_ = maxComfort;
  }

 private:
  MoodResult makeJoyful(const sensing::EnvironmentReadings& env);
  MoodResult makeThirsty(const sensing::EnvironmentReadings& env);
  MoodResult makeOverwatered(const sensing::EnvironmentReadings& env);
  MoodResult makeSleepy(const sensing::EnvironmentReadings& env);
  MoodResult makeLightHungry(const sensing::EnvironmentReadings& env);
  MoodResult makeTooBright(const sensing::EnvironmentReadings& env);
  MoodResult makeTooHot(const sensing::EnvironmentReadings& env);
  MoodResult makeTooCold(const sensing::EnvironmentReadings& env);
  MoodResult makeCurious(const sensing::EnvironmentReadings& env);

  float soilDryThreshold_ = 35.0f;
  float soilSoggyThreshold_ = 85.0f;
  float lightLowThreshold_ = 25.0f;
  float lightHighThreshold_ = 90.0f;
  float comfortTempMinC_ = 17.0f;
  float comfortTempMaxC_ = 28.0f;
  bool lastHydrationAlert_ = false;
  bool lastCelebration_ = false;
};

}  // namespace brain
