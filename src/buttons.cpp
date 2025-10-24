#include "buttons.h"

namespace input {

Button::Button(uint8_t pin, ButtonId id, bool activeLow, uint16_t debounceMs, uint16_t longPressMs)
    : pin_(pin),
      id_(id),
      activeLow_(activeLow),
      debounceMs_(debounceMs),
      longPressMs_(longPressMs) {}

void Button::begin() {
  pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT);
  stableState_ = activeLow_ ? (digitalRead(pin_) == LOW) : (digitalRead(pin_) == HIGH);
  lastReading_ = stableState_;
  lastDebounceMs_ = millis();
  pressedAtMs_ = 0;
  longPressSent_ = false;
}

ButtonEvent Button::update(uint32_t nowMs) {
  bool reading = digitalRead(pin_);
  if (activeLow_) {
    reading = !reading;
  }

  if (reading != lastReading_) {
    lastDebounceMs_ = nowMs;
    lastReading_ = reading;
  }

  if ((nowMs - lastDebounceMs_) < debounceMs_) {
    return {};
  }

  ButtonEvent event;
  event.id = id_;

  if (reading != stableState_) {
    stableState_ = reading;
    if (stableState_) {
      // Button pressed.
      pressedAtMs_ = nowMs;
      longPressSent_ = false;
      event.type = ButtonEventType::Pressed;
      return event;
    }
    // Button released.
    event.type = ButtonEventType::Released;
    if (!longPressSent_ && pressedAtMs_ != 0 && (nowMs - pressedAtMs_) >= debounceMs_) {
      event.type = ButtonEventType::Click;
    }
    pressedAtMs_ = 0;
    longPressSent_ = false;
    return event;
  }

  if (stableState_ && !longPressSent_ && pressedAtMs_ != 0 && (nowMs - pressedAtMs_) >= longPressMs_) {
    longPressSent_ = true;
    event.type = ButtonEventType::LongPress;
    return event;
  }

  return {};
}

ButtonInput::ButtonInput(uint8_t leftPin, uint8_t rightPin, bool activeLow, uint16_t debounceMs, uint16_t longPressMs)
    : left_(leftPin, ButtonId::Left, activeLow, debounceMs, longPressMs),
      right_(rightPin, ButtonId::Right, activeLow, debounceMs, longPressMs) {}

void ButtonInput::begin() {
  left_.begin();
  right_.begin();
}

ButtonEvent ButtonInput::poll() {
  uint32_t now = millis();
  ButtonEvent evt = left_.update(now);
  if (evt.type != ButtonEventType::None) {
    return evt;
  }
  return right_.update(now);
}

}  // namespace input
