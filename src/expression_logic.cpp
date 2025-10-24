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
  mood.face.title = "Feeling great!";
  mood.face.subtitle = kTipHappy;
  mood.face.mouthLevel = 3;
  mood.face.eyelidLevel = -1;
  mood.face.eyesHappy = true;
  mood.face.showSparkle = true;
  mood.face.showHeart = true;
  mood.tip = kTipHappy;
  return mood;
}

MoodResult ExpressionLogic::makeThirsty(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Thirsty;
  mood.face.title = "I'm thirsty...";
  mood.face.subtitle = kTipThirsty;
  mood.face.mouthLevel = -2;
  mood.face.eyelidLevel = 3;
  mood.face.eyesHappy = false;
  mood.face.showTear = true;
  mood.tip = kTipThirsty;
  return mood;
}

MoodResult ExpressionLogic::makeOverwatered(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Overwatered;
  mood.face.title = "Too soggy!";
  mood.face.subtitle = kTipSoggy;
  mood.face.mouthLevel = -3;
  mood.face.eyelidLevel = 2;
  mood.face.eyesHappy = false;
  mood.face.showSweat = true;
  mood.tip = kTipSoggy;
  return mood;
}

MoodResult ExpressionLogic::makeSleepy(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Sleepy;
  mood.face.title = "Sleepy time";
  mood.face.subtitle = kTipSleepy;
  mood.face.mouthLevel = 0;
  mood.face.eyelidLevel = 4;
  mood.face.eyesHappy = true;
  mood.tip = kTipSleepy;
  return mood;
}

MoodResult ExpressionLogic::makeLightHungry(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::SeekingLight;
  mood.face.title = "Need sunshine";
  mood.face.subtitle = kTipLightHungry;
  mood.face.mouthLevel = -1;
  mood.face.eyelidLevel = 1;
  mood.face.eyesHappy = false;
  mood.tip = kTipLightHungry;
  return mood;
}

MoodResult ExpressionLogic::makeTooBright(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooBright;
  mood.face.title = "Too bright!";
  mood.face.subtitle = kTipTooBright;
  mood.face.mouthLevel = -1;
  mood.face.eyelidLevel = 2;
  mood.face.eyesHappy = false;
  mood.face.showSweat = true;
  mood.tip = kTipTooBright;
  return mood;
}

MoodResult ExpressionLogic::makeTooHot(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooHot;
  mood.face.title = "I'm overheating";
  mood.face.subtitle = kTipTooHot;
  mood.face.mouthLevel = -2;
  mood.face.eyelidLevel = 2;
  mood.face.eyesHappy = false;
  mood.face.showSweat = true;
  mood.tip = kTipTooHot;
  return mood;
}

MoodResult ExpressionLogic::makeTooCold(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::TooCold;
  mood.face.title = "Brrr...";
  mood.face.subtitle = kTipTooCold;
  mood.face.mouthLevel = -1;
  mood.face.eyelidLevel = 1;
  mood.face.eyesHappy = false;
  mood.tip = kTipTooCold;
  return mood;
}

MoodResult ExpressionLogic::makeCurious(const sensing::EnvironmentReadings& env) {
  MoodResult mood;
  mood.mood = MoodKind::Curious;
  mood.face.title = "Hello!";
  mood.face.subtitle = kTipCurious;
  mood.face.mouthLevel = 1;
  mood.face.eyelidLevel = 0;
  mood.face.eyesHappy = true;
  mood.tip = kTipCurious;
  return mood;
}

}  // namespace brain
