#include "menu_controller.h"

#include <algorithm>

#include "display_manager.h"
#include "hardware_config.h"
#include "logging.h"

namespace ui {
namespace {

enum class MenuNodeId : uint8_t { Root = 0, SensorTools = 1, PlantTools = 2, DisplaySound = 3 };

enum class MenuItemType : uint8_t { Screen, Submenu, Action, Back };

enum class ActionId : uint8_t {
  None,
  FetchProfile,
  NextPresetFetch,
  MarkSoilDry,
  MarkSoilWet,
  MarkLightDark,
  MarkLightBright,
  PlayDemo,
  ResetProfile,
};

struct MenuItemDef {
  const char* label;
  MenuItemType type;
  MenuNodeId submenu;
  display::PageId screen;
  ActionId action;
};

struct MenuNodeDef {
  MenuNodeId id;
  const char* title;
  const MenuItemDef* items;
  uint8_t itemCount;
};

constexpr MenuItemDef kRootItems[] = {
    {"Face view", MenuItemType::Screen, MenuNodeId::Root, display::PageId::Mood, ActionId::None},
    {"Plant insights", MenuItemType::Screen, MenuNodeId::Root, display::PageId::Info, ActionId::None},
    {"Sensor toolkit", MenuItemType::Submenu, MenuNodeId::SensorTools, display::PageId::Mood, ActionId::None},
    {"Plant toolkit", MenuItemType::Submenu, MenuNodeId::PlantTools, display::PageId::Mood, ActionId::None},
    {"Sound & calm", MenuItemType::Submenu, MenuNodeId::DisplaySound, display::PageId::Mood, ActionId::None},
    {"Diagnostics", MenuItemType::Screen, MenuNodeId::Root, display::PageId::Debug, ActionId::None},
};

constexpr MenuItemDef kSensorItems[] = {
    {"Mark soil as dry", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::MarkSoilDry},
    {"Mark soil as wet", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::MarkSoilWet},
    {"Mark light as dark", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::MarkLightDark},
    {"Mark light as bright", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::MarkLightBright},
    {"Back", MenuItemType::Back, MenuNodeId::Root, display::PageId::Mood, ActionId::None},
};

constexpr MenuItemDef kPlantItems[] = {
    {"Fetch plant profile", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::FetchProfile},
    {"Next preset + fetch", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::NextPresetFetch},
    {"Reset plant profile", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::ResetProfile},
    {"Back", MenuItemType::Back, MenuNodeId::Root, display::PageId::Mood, ActionId::None},
};

constexpr MenuItemDef kDisplayItems[] = {
    {"Play audio demo", MenuItemType::Action, MenuNodeId::Root, display::PageId::Mood, ActionId::PlayDemo},
    {"Back", MenuItemType::Back, MenuNodeId::Root, display::PageId::Mood, ActionId::None},
};

constexpr MenuNodeDef kMenuNodes[] = {
    {MenuNodeId::Root, "Main menu", kRootItems, static_cast<uint8_t>(sizeof(kRootItems) / sizeof(MenuItemDef))},
    {MenuNodeId::SensorTools, "Sensor tools", kSensorItems, static_cast<uint8_t>(sizeof(kSensorItems) / sizeof(MenuItemDef))},
    {MenuNodeId::PlantTools, "Plant tools", kPlantItems, static_cast<uint8_t>(sizeof(kPlantItems) / sizeof(MenuItemDef))},
    {MenuNodeId::DisplaySound, "Sound & calm", kDisplayItems,
     static_cast<uint8_t>(sizeof(kDisplayItems) / sizeof(MenuItemDef))},
};

const MenuNodeDef& nodeForIndex(uint8_t index) {
  return kMenuNodes[index % (sizeof(kMenuNodes) / sizeof(MenuNodeDef))];
}

const MenuNodeDef& nodeForId(MenuNodeId id) {
  return nodeForIndex(static_cast<uint8_t>(id));
}

display::MenuEntryKind kindForItem(MenuItemType type) {
  switch (type) {
    case MenuItemType::Submenu:
      return display::MenuEntryKind::Submenu;
    case MenuItemType::Screen:
      return display::MenuEntryKind::Screen;
    case MenuItemType::Action:
      return display::MenuEntryKind::Action;
    case MenuItemType::Back:
    default:
      return display::MenuEntryKind::Back;
  }
}

}  // namespace

constexpr const char* kLogTagMenu = "menu";

void MenuController::begin(const display::PageId* screens, uint8_t screenCount) {
  screens_ = screens;
  screenCount_ = screenCount;
  activeScreenIndex_ = 0;
  if (screens_ != nullptr && screenCount_ > 0) {
    activeScreen_ = screens_[0];
  } else {
    activeScreen_ = display::PageId::Mood;
  }
  depth_ = 1;
  stack_[0].menuIndex = static_cast<uint8_t>(MenuNodeId::Root);
  stack_[0].selection = 0;
  inMenu_ = true;
  syncState();
  LOG_INFO(kLogTagMenu, "Menu initialized with %u screens", screenCount_);
}

MenuAction MenuController::handleEvent(const input::ButtonEvent& event) {
  MenuAction action;
  action.screen = activeScreen_;
  if (event.type == input::ButtonEventType::None) {
    return action;
  }

  if (inMenu_) {
    if (event.type == input::ButtonEventType::Click || event.type == input::ButtonEventType::LongPress) {
      if (event.id == input::ButtonId::Left) {
        moveSelection(-1);
      } else if (event.id == input::ButtonId::Right) {
        moveSelection(+1);
      } else if (event.id == input::ButtonId::Both) {
        action = activateSelection();
        if (action.openScreen) {
          action.screen = activeScreen_;
        }
      }
      LOG_DEBUG(kLogTagMenu, "Menu input id=%d type=%d sel=%u depth=%u",
                static_cast<int>(event.id), static_cast<int>(event.type),
                stack_[depth_ - 1].selection, depth_);
  }
    return action;
  }

  if (!inMenu_) {
    if (event.type == input::ButtonEventType::Click || event.type == input::ButtonEventType::LongPress) {
      if (event.id == input::ButtonId::Left || event.id == input::ButtonId::Right || event.id == input::ButtonId::Both) {
        inMenu_ = true;
        syncState();
        action.returnToMenu = true;
        LOG_INFO(kLogTagMenu, "Menu opened from face view");
      }
    }
    return action;
  }

  return action;
}

void MenuController::buildMenuView(display::MenuListView* view) const {
  if (view == nullptr) {
    return;
  }
  view->entryCount = 0;
  view->totalCount = 0;
  view->selectedIndex = 0;
  view->topIndex = 0;
  view->title = nullptr;
  for (uint8_t i = 0; i < display::MenuListView::kMaxVisible; ++i) {
    view->items[i] = nullptr;
    view->kinds[i] = display::MenuEntryKind::Action;
  }

  if (!inMenu_ || depth_ == 0) {
    return;
  }

  const StackEntry& top = stack_[depth_ - 1];
  const MenuNodeDef& node = nodeForIndex(top.menuIndex);
  view->title = node.title;
  view->totalCount = node.itemCount;
  view->selectedIndex = std::min<uint8_t>(top.selection, node.itemCount > 0 ? node.itemCount - 1 : 0);

  if (node.itemCount == 0) {
    return;
  }

  uint8_t maxVisible = display::MenuListView::kMaxVisible;
  uint8_t topIndex = 0;

  if (node.itemCount <= maxVisible) {
    topIndex = 0;
  } else {
    if (view->selectedIndex >= maxVisible) {
      topIndex = view->selectedIndex - (maxVisible - 1);
    }
    uint8_t maxTop = node.itemCount - maxVisible;
    if (topIndex > maxTop) {
      topIndex = maxTop;
    }
  }

  view->topIndex = topIndex;
  view->entryCount = std::min<uint8_t>(maxVisible, node.itemCount - topIndex);
  for (uint8_t i = 0; i < view->entryCount; ++i) {
    const MenuItemDef& item = node.items[topIndex + i];
    view->items[i] = item.label;
    view->kinds[i] = kindForItem(item.type);
  }
}

void MenuController::moveSelection(int8_t delta) {
  if (depth_ == 0) {
    return;
  }
  StackEntry& top = stack_[depth_ - 1];
  const MenuNodeDef& node = nodeForIndex(top.menuIndex);
  if (node.itemCount == 0) {
    top.selection = 0;
    return;
  }
  int16_t index = static_cast<int16_t>(top.selection) + delta;
  while (index < 0) {
    index += node.itemCount;
  }
  while (index >= node.itemCount) {
    index -= node.itemCount;
  }
  top.selection = static_cast<uint8_t>(index);
}

MenuAction MenuController::activateSelection() {
  MenuAction action;
  if (depth_ == 0) {
    return action;
  }
  StackEntry& top = stack_[depth_ - 1];
  const MenuNodeDef& node = nodeForIndex(top.menuIndex);
  if (node.itemCount == 0 || top.selection >= node.itemCount) {
    return action;
  }

  const MenuItemDef& item = node.items[top.selection];

  switch (item.type) {
    case MenuItemType::Screen:
      enterScreen(item.screen);
      action.openScreen = true;
      action.screen = activeScreen_;
      LOG_INFO(kLogTagMenu, "Opening screen %u via menu", static_cast<unsigned>(item.screen));
      break;
    case MenuItemType::Submenu:
      pushMenu(static_cast<uint8_t>(item.submenu));
      LOG_DEBUG(kLogTagMenu, "Entering submenu %u", static_cast<unsigned>(item.submenu));
      break;
    case MenuItemType::Action:
      switch (item.action) {
        case ActionId::FetchProfile:
          action.triggerProfileFetch = true;
          action.presetDelta = 0;
          LOG_INFO(kLogTagMenu, "Action fetch profile");
          break;
        case ActionId::NextPresetFetch:
          action.triggerProfileFetch = true;
          action.presetDelta = 1;
          LOG_INFO(kLogTagMenu, "Action next preset + fetch");
          break;
        case ActionId::MarkSoilDry:
          action.calibration = CalibrationTarget::SoilDry;
          LOG_INFO(kLogTagMenu, "Action calibrate soil dry");
          break;
        case ActionId::MarkSoilWet:
          action.calibration = CalibrationTarget::SoilWet;
          LOG_INFO(kLogTagMenu, "Action calibrate soil wet");
          break;
        case ActionId::MarkLightDark:
          action.calibration = CalibrationTarget::LightDark;
          LOG_INFO(kLogTagMenu, "Action calibrate light dark");
          break;
        case ActionId::MarkLightBright:
          action.calibration = CalibrationTarget::LightBright;
          LOG_INFO(kLogTagMenu, "Action calibrate light bright");
          break;
        case ActionId::PlayDemo:
          action.playDemoChord = true;
          LOG_INFO(kLogTagMenu, "Action play demo");
          break;
        case ActionId::ResetProfile:
          action.resetProfile = true;
          LOG_WARN(kLogTagMenu, "Action reset profile");
          break;
        case ActionId::None:
        default:
          break;
      }
      break;
    case MenuItemType::Back:
    default:
      popMenu();
      LOG_DEBUG(kLogTagMenu, "Back to parent menu, depth now %u", depth_);
      break;
  }

  return action;
}

void MenuController::enterScreen(display::PageId screen) {
  inMenu_ = false;
  for (uint8_t i = 0; i < screenCount_; ++i) {
    if (screens_[i] == screen) {
      activeScreenIndex_ = i;
      break;
    }
  }
  activeScreen_ = screen;
  syncState();
  LOG_INFO(kLogTagMenu, "Entered screen index %u (id %u)", activeScreenIndex_, static_cast<unsigned>(screen));
}

void MenuController::nextScreen() {
  if (screenCount_ == 0) {
    return;
  }
  activeScreenIndex_ = (activeScreenIndex_ + 1) % screenCount_;
  activeScreen_ = screens_[activeScreenIndex_];
  syncState();
  LOG_DEBUG(kLogTagMenu, "Next screen -> index %u (id %u)", activeScreenIndex_, static_cast<unsigned>(activeScreen_));
}

void MenuController::previousScreen() {
  if (screenCount_ == 0) {
    return;
  }
  if (activeScreenIndex_ == 0) {
    activeScreenIndex_ = screenCount_ - 1;
  } else {
    --activeScreenIndex_;
  }
  activeScreen_ = screens_[activeScreenIndex_];
  syncState();
  LOG_DEBUG(kLogTagMenu, "Previous screen -> index %u (id %u)", activeScreenIndex_,
            static_cast<unsigned>(activeScreen_));
}

void MenuController::pushMenu(uint8_t submenuIndex) {
  if (depth_ >= kMaxDepth) {
    LOG_WARN(kLogTagMenu, "Max menu depth reached");
    return;
  }
  stack_[depth_].menuIndex = submenuIndex % (sizeof(kMenuNodes) / sizeof(MenuNodeDef));
  stack_[depth_].selection = 0;
  ++depth_;
  LOG_DEBUG(kLogTagMenu, "Pushed submenu %u depth=%u", submenuIndex, depth_);
}

void MenuController::popMenu() {
  if (depth_ <= 1) {
    return;
  }
  --depth_;
  LOG_DEBUG(kLogTagMenu, "Popped to depth=%u", depth_);
}

void MenuController::syncState() {
  state_.inMenu = inMenu_;
  state_.activeScreen = activeScreen_;
  state_.screenIndex = activeScreenIndex_;
  state_.screenCount = screenCount_;
}

}  // namespace ui


