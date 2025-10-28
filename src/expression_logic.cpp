#include "expression_logic.h"

#include <cmath>

namespace brain {
namespace {
constexpr const char* kTipHappy = "Everything feels balanced!";
constexpr const char* kTipThirsty = "Please water the plant soon.";
constexpr const char* kTipSoggy = "Let the soil dry before watering.";
constexpr const char* kTipSleepy = "Dim light -> nap time.";
constexpr const char* kTipLightHungry = "Move me closer to the window.";
constexpr const char* kTipTooBright = "Shade me or rotate the pot.";
constexpr const char* kTipTooHot = "Hot! Improve airflow.";
constexpr const char* kTipTooCold = "Feeling chilly, move indoors.";
constexpr const char* kTipCurious = "Sensors calibrating...";

bool isValid(float v) {
  return !std::isnan(v);
}
}  // namespace

MoodResult ExpressionLogic::evaluate(const sensing::EnvironmentReadings& env) {
  bool soilValid = env.soilValid && isValid(env.soilMoisturePct);
  bool lightValid = env.lightValid && isValid(env.lightPct);
  bool tempValid = env.climateValid && isValid(env.temperatureC);

  bool isDry = soilValid && env.soilMoisturePct <= soilDryThreshold_;
  bool isSoggy = soilValid && env.soilMoisturePct >= soilSoggyThreshold_;
  bool tooHot = tempValid && env.temperatureC >= (comfortTempMaxC_ + 2.0f);
  bool tooCold = tempValid && env.temperatureC <= (comfortTempMinC_ - 2.0f);
  bool needsLight = lightValid && env.lightPct <= lightLowThreshold_;
  bool tooBright = lightValid && env.lightPct >= lightHighThreshold_;
  bool sleepy = lightValid && env.lightPct < (lightLowThreshold_ + 8.0f) &&
                (!tempValid || env.temperatureC < comfortTempMinC_ + 1.5f);

  bool celebratory = soilValid && !isDry && !isSoggy && lightValid && !needsLight && !tooBright &&
                     tempValid && env.temperatureC > comfortTempMinC_ && env.temperatureC < comfortTempMaxC_;

  MoodResult result;

  if (isDry) {
    result = makeThirsty(env);
  } else if (isSoggy) {
    result = makeOverwatered(env);
  } else if (tooHot) {
    result = makeTooHot(env);
  } else if (tooCold) {
    result = makeTooCold(env);
  } else if (needsLight) {
    result = makeLightHungry(env);
  } else if (tooBright) {
    result = makeTooBright(env);
  } else if (sleepy) {
    result = makeSleepy(env);
  } else if (soilValid || lightValid || tempValid) {
    result = makeJoyful(env);
  } else {
    result = makeCurious(env);
  }

  result.playHydrationCue = isDry && !lastHydrationAlert_;
  result.playCelebrationCue = celebratory && !lastCelebration_;

  lastHydrationAlert_ = isDry;
  lastCelebration_ = celebratory;

  return result;
}

MoodResult ExpressionLogic::makeJoyful(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Joyful;
  mood.face.eyeOpenness = 2;
  mood.face.eyeSmile = 3;
  mood.face.mouthCurve = 3;
  mood.face.mouthOpen = 1;
  mood.face.blush = true;
  mood.tip = kTipHappy;
  return mood;
}

MoodResult ExpressionLogic::makeThirsty(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Thirsty;
  mood.face.eyeOpenness = -2;
  mood.face.eyeSmile = -1;
  mood.face.mouthCurve = -3;
  mood.face.mouthOpen = 0;
  mood.face.gazeY = 1;
  mood.tip = kTipThirsty;
  return mood;
}

MoodResult ExpressionLogic::makeOverwatered(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Overwatered;
  mood.face.eyeOpenness = -3;
  mood.face.eyeSmile = -2;
  mood.face.mouthCurve = -3;
  mood.face.mouthOpen = 1;
  mood.face.gazeY = 2;
  mood.tip = kTipSoggy;
  return mood;
}

MoodResult ExpressionLogic::makeSleepy(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Sleepy;
  mood.face.eyeOpenness = -4;
  mood.face.eyeSmile = 1;
  mood.face.mouthCurve = -1;
  mood.face.mouthOpen = 0;
  mood.face.gazeY = 2;
  mood.tip = kTipSleepy;
  return mood;
}

MoodResult ExpressionLogic::makeLightHungry(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::SeekingLight;
  mood.face.eyeOpenness = 0;
  mood.face.eyeSmile = -1;
  mood.face.mouthCurve = -1;
  mood.face.mouthOpen = 0;
  mood.face.gazeY = -2;
  mood.tip = kTipLightHungry;
  return mood;
}

MoodResult ExpressionLogic::makeTooBright(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooBright;
  mood.face.eyeOpenness = -1;
  mood.face.eyeSmile = -3;
  mood.face.mouthCurve = -2;
  mood.face.mouthOpen = 1;
  mood.face.gazeX = -2;
  mood.tip = kTipTooBright;
  return mood;
}

MoodResult ExpressionLogic::makeTooHot(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooHot;
  mood.face.eyeOpenness = -1;
  mood.face.eyeSmile = -2;
  mood.face.mouthCurve = -2;
  mood.face.mouthOpen = 2;
  mood.face.gazeX = 1;
  mood.tip = kTipTooHot;
  return mood;
}

MoodResult ExpressionLogic::makeTooCold(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooCold;
  mood.face.eyeOpenness = 1;
  mood.face.eyeSmile = -1;
  mood.face.mouthCurve = -1;
  mood.face.mouthOpen = 0;
  mood.face.gazeX = 2;
  mood.tip = kTipTooCold;
  return mood;
}

MoodResult ExpressionLogic::makeCurious(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Curious;
  mood.face.eyeOpenness = 1;
  mood.face.eyeSmile = 1;
  mood.face.mouthCurve = 1;
  mood.face.mouthOpen = 1;
  mood.face.winkRight = true;
  mood.tip = kTipCurious;
  return mood;
}

}  // namespace brain
