#pragma once

#include <Arduino.h>

namespace input {

enum class ButtonId : uint8_t { Left = 0, Right = 1 };

enum class ButtonEventType : uint8_t {
  None = 0,
  Pressed,
  Released,
  Click,
  LongPress,
};

struct ButtonEvent {
  ButtonEventType type = ButtonEventType::None;
  ButtonId id = ButtonId::Left;
};

class Button {
 public:
  Button(uint8_t pin, ButtonId id, bool activeLow, uint16_t debounceMs, uint16_t longPressMs);
  void begin();
  ButtonEvent update(uint32_t nowMs);

 private:
  uint8_t pin_;
  ButtonId id_;
  bool activeLow_;
  uint16_t debounceMs_;
  uint16_t longPressMs_;

  bool lastReading_ = false;
  bool stableState_ = false;
  uint32_t lastDebounceMs_ = 0;
  uint32_t pressedAtMs_ = 0;
  bool longPressSent_ = false;
};

class ButtonInput {
 public:
  ButtonInput(uint8_t leftPin, uint8_t rightPin, bool activeLow, uint16_t debounceMs, uint16_t longPressMs);
  void begin();
  ButtonEvent poll();

 private:
  Button left_;
  Button right_;
};

}  // namespace input
