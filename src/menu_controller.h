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
  bool inMenu = true;
  display::PageId activeScreen = display::PageId::Mood;
  uint8_t screenIndex = 0;
  uint8_t screenCount = 0;
};

struct MenuAction {
  bool openScreen = false;
  bool returnToMenu = false;
  display::PageId screen;
  CalibrationTarget calibration = CalibrationTarget::None;
  bool playDemoChord = false;
  bool triggerProfileFetch = false;
  int8_t presetDelta = 0;
  bool resetProfile = false;
};

class MenuController {
 public:
  void begin(const display::PageId* screens, uint8_t screenCount);
  MenuAction handleEvent(const input::ButtonEvent& event);
  const MenuState& state() const { return state_; }
  void buildMenuView(display::MenuListView* view) const;

 private:
  void moveSelection(int8_t delta);
  MenuAction activateSelection();
  void enterScreen(display::PageId screen);
  void nextScreen();
  void previousScreen();
  void pushMenu(uint8_t submenuIndex);
  void popMenu();
  void syncState();

  static constexpr uint8_t kMaxDepth = 4;

  const display::PageId* screens_ = nullptr;
  uint8_t screenCount_ = 0;

  struct StackEntry {
    uint8_t menuIndex = 0;
    uint8_t selection = 0;
  };

  StackEntry stack_[kMaxDepth];
  uint8_t depth_ = 0;
  bool inMenu_ = true;
  display::PageId activeScreen_;
  uint8_t activeScreenIndex_ = 0;
  MenuState state_;
};

}  // namespace ui
