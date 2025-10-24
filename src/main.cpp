#include <Arduino.h>
#include <Wire.h>
#include <esp_random.h>

#include "audio_engine.h"
#include "buttons.h"
#include "display_manager.h"
#include "expression_logic.h"
#include "hardware_config.h"
#include "menu_controller.h"
#include "sensors.h"

namespace {
constexpr uint32_t kSensorIntervalMs = 1500;
constexpr uint32_t kDisplayIntervalMs = 100;
constexpr uint8_t kPageCount = 4;

sensing::SensorSuite sensors;
audio::AudioEngine audioEngine;
display::DisplayManager displayManager;
input::ButtonInput buttons(hw::PIN_BUTTON_LEFT, hw::PIN_BUTTON_RIGHT, /*activeLow=*/true,
                           hw::BUTTON_DEBOUNCE_MS, hw::BUTTON_LONG_PRESS_MS);
brain::ExpressionLogic expressionLogic;
ui::MenuController menuController;

sensing::EnvironmentReadings lastReadings;
brain::MoodResult currentMood;

uint32_t lastSensorSampleMs = 0;
uint32_t lastDisplayUpdateMs = 0;

bool blinkActive = false;
uint32_t blinkStartMs = 0;
uint32_t nextBlinkAtMs = 0;

uint16_t soilDryCalibration = hw::SOIL_RAW_DRY_DEFAULT;
uint16_t soilWetCalibration = hw::SOIL_RAW_WET_DEFAULT;
uint16_t lightDarkCalibration = hw::LIGHT_RAW_DARK_DEFAULT;
uint16_t lightBrightCalibration = hw::LIGHT_RAW_BRIGHT_DEFAULT;

void scheduleNextBlink(uint32_t nowMs) {
  uint32_t window = hw::BLINK_INTERVAL_MAX_MS - hw::BLINK_INTERVAL_MIN_MS;
  nextBlinkAtMs = nowMs + hw::BLINK_INTERVAL_MIN_MS + random(window);
}

bool updateBlink(uint32_t nowMs) {
  if (blinkActive) {
    if (nowMs - blinkStartMs >= hw::BLINK_DURATION_MS) {
      blinkActive = false;
      scheduleNextBlink(nowMs);
    }
  } else if (nowMs >= nextBlinkAtMs) {
    blinkActive = true;
    blinkStartMs = nowMs;
  }
  return blinkActive;
}

void applyCalibration(ui::CalibrationTarget target) {
  switch (target) {
    case ui::CalibrationTarget::SoilDry:
      soilDryCalibration = lastReadings.soilRaw;
      sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
      Serial.printf("[cal] Soil dry raw=%u\n", soilDryCalibration);
      audioEngine.playTone(523.3f, 220);
      break;
    case ui::CalibrationTarget::SoilWet:
      soilWetCalibration = lastReadings.soilRaw;
      sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
      Serial.printf("[cal] Soil wet raw=%u\n", soilWetCalibration);
      audioEngine.playTone(659.3f, 220);
      break;
    case ui::CalibrationTarget::LightDark:
      lightDarkCalibration = lastReadings.lightRaw;
      sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
      Serial.printf("[cal] Light dark raw=%u\n", lightDarkCalibration);
      audioEngine.playTone(392.0f, 180);
      break;
    case ui::CalibrationTarget::LightBright:
      lightBrightCalibration = lastReadings.lightRaw;
      sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
      Serial.printf("[cal] Light bright raw=%u\n", lightBrightCalibration);
      audioEngine.playTone(784.0f, 180);
      break;
    case ui::CalibrationTarget::None:
    default:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("PlanteyPetC3 booting...");

  randomSeed(esp_random());

  buttons.begin();
  sensors.begin();
  sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
  sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
  audioEngine.begin();
  menuController.begin(kPageCount);

  displayManager.begin();
  displayManager.drawSplash("Plantey Pet", "Warming up sensors...");

  lastReadings = sensors.sample();
  currentMood = expressionLogic.evaluate(lastReadings);
  scheduleNextBlink(millis());
}

void loop() {
  uint32_t now = millis();

  // Buttons and menu handling.
  input::ButtonEvent evt = buttons.poll();
  if (evt.type != input::ButtonEventType::None) {
    ui::MenuAction action = menuController.handleEvent(evt);
    if (action.pageChanged) {
      lastDisplayUpdateMs = 0;
    }
    if (action.calibration != ui::CalibrationTarget::None) {
      applyCalibration(action.calibration);
    }
    if (action.contrastDelta != 0) {
      int16_t contrast = static_cast<int16_t>(displayManager.contrast());
      contrast = constrain(contrast + action.contrastDelta, 10, 255);
      displayManager.setContrast(static_cast<uint8_t>(contrast));
    }
    if (action.playDemoChord) {
      audioEngine.playChord({523.3f, 659.3f, 783.9f}, 900, 10);
    }
  }

  // Sensor sampling.
  if (now - lastSensorSampleMs >= kSensorIntervalMs) {
    lastReadings = sensors.sample();
    currentMood = expressionLogic.evaluate(lastReadings);
    if (currentMood.playHydrationCue && !audioEngine.isPlaying()) {
      audioEngine.playChord({392.0f, 523.3f}, 800, 14);
    } else if (currentMood.playCelebrationCue && !audioEngine.isPlaying()) {
      audioEngine.playChord({523.3f, 659.3f, 783.9f}, 750, 8);
    }
    lastSensorSampleMs = now;
  }

  // Display updates, including blink animation.
  bool blink = updateBlink(now);
  if (now - lastDisplayUpdateMs >= kDisplayIntervalMs) {
    const ui::MenuState& menuState = menuController.state();
    displayManager.render(currentMood.face, lastReadings, menuState.page, menuState.pageIndex, menuState.pageCount,
                          blink);
    lastDisplayUpdateMs = now;
  }

  audioEngine.update();
  delay(10);
}
