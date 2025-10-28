#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "hardware_config.h"
#include "plant_profile.h"
#include "sensors.h"

namespace display {

enum class PageId : uint8_t { Mood = 0, Info = 1, Debug = 2, Menu = 3 };

enum class MenuEntryKind : uint8_t { Screen, Submenu, Action, Back };

struct MenuListView {
  static constexpr uint8_t kMaxVisible = 5;
  const char* title = nullptr;
  const char* items[kMaxVisible] = {};
  MenuEntryKind kinds[kMaxVisible] = {};
  uint8_t entryCount = 0;
  uint8_t selectedIndex = 0;
  uint8_t topIndex = 0;
  uint8_t totalCount = 0;
};

struct FaceExpressionView {
  int8_t gazeX = 0;            // -6 .. +6 horizontal pupil offset
  int8_t gazeY = 0;            // -4 .. +4 vertical pupil offset
  int8_t eyeOpenness = 0;      // -4 .. +4 (negative=sleepy, positive=wide)
  int8_t eyeSmile = 0;         // -4 .. +4 (eyelid curvature)
  int8_t mouthCurve = 0;       // -4 .. +4 (negative=frown, positive=smile)
  int8_t mouthOpen = 0;        // 0 .. +4 (jaw drop)
  bool blush = false;
  bool winkLeft = false;
  bool winkRight = false;
  uint16_t interactionPulseMs = 0xFFFF;  // values < ~900ms signal a recent interaction gesture
};

struct SystemStatusView {
  const plant::PlantProfile* profile = nullptr;
  String profileStatus;
  String wifiStatus;
  bool wifiConnected = false;
  bool fetchInProgress = false;
  uint32_t profileAgeSeconds = 0;
};

class DisplayManager {
 public:
  void begin();

  void render(const FaceExpressionView& face,
              const sensing::EnvironmentReadings& environment,
              const SystemStatusView& status,
              const MenuListView* menu,
              const char* timeText,
              PageId page,
              uint8_t pageIndex,
              uint8_t pageCount,
              bool blinkFrame);

  void drawSplash(const char* line1, const char* line2 = nullptr);

 private:
  void drawFaceLayer(const FaceExpressionView& face, bool blinkFrame, const char* timeText);
  void drawMenuLayer(const MenuListView& menu);
  void drawInfoLayer(const sensing::EnvironmentReadings& environment, const SystemStatusView& status);
  void drawDebugLayer(const sensing::EnvironmentReadings& environment, const SystemStatusView& status);
  void drawFooter(PageId page, uint8_t pageIndex, uint8_t pageCount, bool menuVisible);

  U8G2_SH1106_128X64_NONAME_F_HW_I2C display_{U8G2_R0, /* reset=*/U8X8_PIN_NONE, hw::PIN_I2C_SCL, hw::PIN_I2C_SDA};
  bool started_ = false;
};

}  // namespace display
