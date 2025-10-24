#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "hardware_config.h"
#include "sensors.h"

namespace display {

enum class PageId : uint8_t { Mood = 0, Stats = 1, Tips = 2, Debug = 3 };

struct FaceExpressionView {
  const char* title = "Calm";
  const char* subtitle = nullptr;
  int8_t mouthLevel = 0;     // -4 .. +4
  int8_t eyelidLevel = 0;    // -4 .. +4
  bool eyesHappy = true;
  bool showSparkle = false;
  bool showTear = false;
  bool showSweat = false;
  bool showHeart = false;
};

class DisplayManager {
 public:
  void begin();

  void render(const FaceExpressionView& face,
              const sensing::EnvironmentReadings& environment,
              PageId page,
              uint8_t pageIndex,
              uint8_t pageCount,
              bool blinkFrame);

  void drawSplash(const char* line1, const char* line2 = nullptr);
  void setContrast(uint8_t level);
  uint8_t contrast() const { return contrast_; }

 private:
  void drawFaceLayer(const FaceExpressionView& face, bool blinkFrame);
  void drawStatsLayer(const sensing::EnvironmentReadings& environment);
  void drawTipsLayer(const FaceExpressionView& face, const sensing::EnvironmentReadings& environment);
  void drawDebugLayer(const sensing::EnvironmentReadings& environment);
  void drawFooter(PageId page, uint8_t pageIndex, uint8_t pageCount);

  U8G2_SH1106_128X64_NONAME_F_HW_I2C display_{U8G2_R0, /* reset=*/U8X8_PIN_NONE, hw::PIN_I2C_SCL, hw::PIN_I2C_SDA};
  bool started_ = false;
  uint8_t contrast_ = 180;
};

}  // namespace display
