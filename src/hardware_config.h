#pragma once

#include <Arduino.h>

namespace hw {

// Pin map for the ESP32-C3 Super Mini (adjust if using a different variant).
constexpr uint8_t PIN_SOIL_SENSOR = 0;    // ADC1_CH0
constexpr uint8_t PIN_LDR_SENSOR = 1;     // ADC1_CH1
constexpr uint8_t PIN_DHT = 3;
constexpr uint8_t PIN_BUTTON_LEFT = 6;
constexpr uint8_t PIN_BUTTON_RIGHT = 7;
constexpr uint8_t PIN_BUZZER = 2;
constexpr uint8_t PIN_I2C_SDA = 8;
constexpr uint8_t PIN_I2C_SCL = 9;

// LEDC (PWM) configuration for the piezo buzzer.
constexpr uint8_t BUZZER_LEDC_CHANNEL = 0;
constexpr uint8_t BUZZER_LEDC_TIMER = 0;
constexpr uint8_t BUZZER_LEDC_RESOLUTION = 10;  // bits

// Sensor calibration defaults. These are ballpark values and should be refined
// using the calibration helper menu.
constexpr uint16_t SOIL_RAW_DRY_DEFAULT = 3200;  // higher value => drier
constexpr uint16_t SOIL_RAW_WET_DEFAULT = 1500;  // lower value => wetter
constexpr uint16_t LIGHT_RAW_DARK_DEFAULT = 3500;
constexpr uint16_t LIGHT_RAW_BRIGHT_DEFAULT = 200;

// Smoothing behaviour.
constexpr float SOIL_ALPHA = 0.10f;   // EMA smoothing factor
constexpr float LIGHT_ALPHA = 0.10f;

// Button behaviour.
constexpr uint16_t BUTTON_DEBOUNCE_MS = 35;
constexpr uint16_t BUTTON_LONG_PRESS_MS = 700;

// Display refresh periods.
constexpr uint16_t FACE_FRAME_INTERVAL_MS = 90;
constexpr uint16_t BLINK_INTERVAL_MIN_MS = 2500;
constexpr uint16_t BLINK_INTERVAL_MAX_MS = 6000;
constexpr uint16_t BLINK_DURATION_MS = 160;

}  // namespace hw
