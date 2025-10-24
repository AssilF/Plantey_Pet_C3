#include "menu_controller.h"

#include "hardware_config.h"

namespace ui {

void MenuController::begin(uint8_t pageCount) {
  state_.pageCount = pageCount;
  state_.pageIndex = 0;
  state_.page = static_cast<display::PageId>(0);
}

MenuAction MenuController::handleEvent(const input::ButtonEvent& event) {
  MenuAction action;
  if (event.type == input::ButtonEventType::None) {
    return action;
  }

  switch (event.type) {
    case input::ButtonEventType::Click: {
      bool changed = false;
      if (event.id == input::ButtonId::Left) {
        changePage(-1);
        changed = true;
      } else if (event.id == input::ButtonId::Right) {
        changePage(+1);
        changed = true;
      }
      action.pageChanged = changed;
      break;
    }

    case input::ButtonEventType::LongPress:
      if (state_.page == display::PageId::Stats) {
        action.calibration = (event.id == input::ButtonId::Left) ? CalibrationTarget::SoilDry
                                                                 : CalibrationTarget::SoilWet;
      } else if (state_.page == display::PageId::Tips) {
        action.calibration = (event.id == input::ButtonId::Left) ? CalibrationTarget::LightDark
                                                                 : CalibrationTarget::LightBright;
      } else if (state_.page == display::PageId::Mood) {
        action.playDemoChord = true;
      } else if (state_.page == display::PageId::Debug) {
        action.contrastDelta = (event.id == input::ButtonId::Left) ? -10 : +10;
      }
      break;

    case input::ButtonEventType::Pressed:
    case input::ButtonEventType::Released:
    case input::ButtonEventType::None:
    default:
      break;
  }

  return action;
}

void MenuController::changePage(int8_t delta) {
  if (state_.pageCount == 0) {
    return;
  }
  int16_t index = state_.pageIndex + delta;
  if (index < 0) {
    index = state_.pageCount - 1;
  } else if (index >= state_.pageCount) {
    index = 0;
  }
  state_.pageIndex = static_cast<uint8_t>(index);
  state_.page = static_cast<display::PageId>(state_.pageIndex);
}

}  // namespace ui
