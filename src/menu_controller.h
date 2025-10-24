#pragma once

#include "buttons.h"
#include "display_manager.h"

namespace ui {

enum class CalibrationTarget : uint8_t {
  None,
  SoilDry,
  SoilWet,
  LightDark,
  LightBright,
};

struct MenuState {
  display::PageId page = display::PageId::Mood;
  uint8_t pageIndex = 0;
  uint8_t pageCount = 4;
};

struct MenuAction {
  bool pageChanged = false;
  CalibrationTarget calibration = CalibrationTarget::None;
  int8_t contrastDelta = 0;
  bool playDemoChord = false;
};

class MenuController {
 public:
  void begin(uint8_t pageCount);
  MenuAction handleEvent(const input::ButtonEvent& event);
  const MenuState& state() const { return state_; }

 private:
  void changePage(int8_t delta);
  MenuState state_;
};

}  // namespace ui
