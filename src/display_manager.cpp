#include "display_manager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace display {
namespace {

constexpr const char* kPageTitles[] = {"Face", "Info", "Debug"};
constexpr size_t kPageTitleCount = sizeof(kPageTitles) / sizeof(const char*);
constexpr float kTwoPi = 6.28318530718f;

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

}  // namespace

constexpr uint8_t MenuListView::kMaxVisible;

void DisplayManager::begin() {
  if (started_) {
    return;
  }
  display_.begin();
  started_ = true;
}

void DisplayManager::render(const FaceExpressionView& face,
                            const sensing::EnvironmentReadings& environment,
                            const SystemStatusView& status,
                            const MenuListView* menu,
                            const char* timeText,
                            PageId page,
                            uint8_t pageIndex,
                            uint8_t pageCount,
                            bool blinkFrame) {
  if (!started_) {
    begin();
  }

  display_.clearBuffer();
  switch (page) {
    case PageId::Menu:
      if (menu != nullptr) {
        drawMenuLayer(*menu);
      }
      break;
    case PageId::Mood:
      drawFaceLayer(face, blinkFrame, timeText);
      break;
    case PageId::Info:
      drawInfoLayer(environment, status);
      break;
    case PageId::Debug:
      drawDebugLayer(environment, status);
      break;
  }
  drawFooter(page, pageIndex, pageCount, page == PageId::Menu);
  display_.sendBuffer();
}

void DisplayManager::drawSplash(const char* line1, const char* line2) {
  if (!started_) {
    begin();
  }
  display_.clearBuffer();
  display_.setFont(u8g2_font_fub14_tf);
  const char* logo = (line1 != nullptr && line1[0] != '\0') ? line1 : "Plantey";
  int16_t width = display_.getStrWidth(logo);
  display_.drawStr((128 - width) / 2, 38, logo);

  display_.drawTriangle(60, 16, 68, 6, 76, 16);
  display_.drawLine(68, 6, 68, 22);
  display_.drawCircle(68, 24, 2);

  if (line2 != nullptr && line2[0] != '\0') {
    display_.setFont(u8g2_font_6x12_tf);
    width = display_.getStrWidth(line2);
    display_.drawStr((128 - width) / 2, 58, line2);
  } else {
    display_.setFont(u8g2_font_6x10_tf);
    display_.drawStr(32, 58, "gently growing");
  }
  display_.sendBuffer();
}

void DisplayManager::drawFaceLayer(const FaceExpressionView& face, bool blinkFrame, const char* timeText) {
  (void)timeText;  // default face screen stays wordless

  auto clampFloat = [](float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
  };

  uint32_t now = millis();
  float breath = std::sin((now % 5200UL) * kTwoPi / 5200.0f);
  float sway = std::sin((now % 8700UL) * kTwoPi / 8700.0f);

  float interaction = 0.0f;
  if (face.interactionPulseMs != 0xFFFF) {
    float t = clampFloat(static_cast<float>(face.interactionPulseMs) / 900.0f, 0.0f, 1.0f);
    interaction = 1.0f - t;
  }

  float baseOpen = clampFloat((static_cast<float>(clampInt(face.eyeOpenness, -4, 4)) + 4.0f) / 8.0f, 0.05f, 1.25f);
  float blinkScale = blinkFrame ? 0.08f : 1.0f;
  float openValue = clampFloat(baseOpen * (0.85f + breath * 0.08f + interaction * 0.25f) * blinkScale, 0.05f, 1.4f);

  float eyeSmileFactor = clampFloat(static_cast<float>(clampInt(face.eyeSmile, -4, 4)) / 4.0f, -1.0f, 1.0f);

  bool interactionAnimation = (face.interactionPulseMs != 0xFFFF) && (face.interactionPulseMs < 320);
  bool interactionHalf = interactionAnimation && ((face.interactionPulseMs / 80U) % 2U == 0U);

  bool winkLeft = face.winkLeft || (interactionAnimation && interactionHalf);
  bool winkRight = face.winkRight || (interactionAnimation && !interactionHalf);

  int16_t centerX = 64 + clampInt(face.gazeX, -6, 6) / 2 + static_cast<int16_t>(sway * 1.5f);
  int16_t centerY = 34 + static_cast<int16_t>(breath * 2.0f) - static_cast<int16_t>(interaction * 3.0f);

  int16_t eyeSpacing = 36;
  int16_t eyeWidth = 28 + static_cast<int16_t>(interaction * 4.0f);
  int16_t eyeBaseHeight = 12;
  int16_t gazeOffsetX = clampInt(face.gazeX, -6, 6);
  int16_t gazeOffsetY = clampInt(face.gazeY, -4, 4);

  auto drawEye = [&](int16_t cx, float openness, bool winkFlag) {
    float localOpen = winkFlag ? 0.08f : openness;
    int16_t height = std::max<int16_t>(3, static_cast<int16_t>(std::round(6.0f + localOpen * eyeBaseHeight)));
    int16_t top = centerY - 18 + gazeOffsetY;
    int16_t left = cx - eyeWidth / 2;

    if (localOpen <= 0.12f) {
      int16_t y = top + height / 2;
      display_.setDrawColor(1);
      display_.drawLine(left + 2, y, left + eyeWidth - 2, y);
      if (eyeSmileFactor > 0.25f) {
        display_.drawLine(left + 2, y + 1, left + 8, y + 2);
        display_.drawLine(left + eyeWidth - 2, y + 1, left + eyeWidth - 8, y + 2);
      } else if (eyeSmileFactor < -0.25f) {
        display_.drawLine(left + 3, y - 1, left + eyeWidth - 3, y - 3);
      }
      return;
    }

    display_.setDrawColor(1);
    display_.drawRBox(left, top, eyeWidth, height, 4);

    display_.setDrawColor(0);
    int16_t pupilW = 8 + static_cast<int16_t>(interaction * 4.0f);
    int16_t pupilH = std::max<int16_t>(3, height - 4);
    int16_t pupilLeft = cx - pupilW / 2 + (gazeOffsetX * 2) / 3;
    int16_t pupilTop = top + (height - pupilH) / 2 + gazeOffsetY / 2;
    display_.drawRBox(pupilLeft, pupilTop, pupilW, pupilH, 3);

    display_.setDrawColor(1);
    display_.drawRFrame(left, top, eyeWidth, height, 4);

    if (eyeSmileFactor > 0.25f) {
      display_.drawLine(left + 2, top + height, left + 8, top + height + 1);
      display_.drawLine(left + eyeWidth - 2, top + height, left + eyeWidth - 8, top + height + 1);
    } else if (eyeSmileFactor < -0.25f) {
      display_.drawLine(left + 2, top + 1, left + 10, top - 2);
      display_.drawLine(left + eyeWidth - 2, top + 1, left + eyeWidth - 10, top - 2);
    }
  };

  drawEye(centerX - eyeSpacing, openValue, winkLeft);
  drawEye(centerX + eyeSpacing, openValue, winkRight);

  if (face.blush || interaction > 0.4f) {
    int16_t blushY = centerY + 4;
    for (int dx = -12; dx <= 12; dx += 4) {
      display_.drawPixel(centerX - eyeSpacing + dx, blushY);
      display_.drawPixel(centerX + eyeSpacing + dx, blushY + (((dx / 4) & 1) ? 1 : 0));
    }
  }

  float mouthCurve = clampFloat(static_cast<float>(clampInt(face.mouthCurve, -4, 4)) / 4.0f, -1.0f, 1.0f);
  float mouthOpen = clampFloat(static_cast<float>(clampInt(face.mouthOpen, 0, 4)) / 4.0f, 0.0f, 1.0f);
  mouthOpen = clampFloat(mouthOpen + interaction * 0.3f, 0.0f, 1.3f);

  int16_t mouthWidth = 54 + static_cast<int16_t>(interaction * 6.0f);
  int16_t mouthHeight = std::max<int16_t>(3, static_cast<int16_t>(std::round(5.0f + mouthOpen * 10.0f)));
  int16_t mouthOffsetY = static_cast<int16_t>(-mouthCurve * 4.0f);
  int16_t mouthCenterY = centerY + 18 + mouthOffsetY;
  int16_t mouthTop = mouthCenterY - mouthHeight / 2;
  int16_t mouthLeft = centerX - mouthWidth / 2;

  display_.setDrawColor(1);
  display_.drawRBox(mouthLeft, mouthTop, mouthWidth, mouthHeight, 6);

  display_.setDrawColor(0);
  int16_t innerHeight = std::max<int16_t>(2, mouthHeight - 4);
  display_.drawRBox(mouthLeft + 2, mouthTop + 2, mouthWidth - 4, innerHeight, 4);

  display_.setDrawColor(1);
  if (mouthCurve > 0.25f) {
    display_.drawLine(mouthLeft, mouthTop + mouthHeight - 1, mouthLeft + 6, mouthTop + mouthHeight + 1);
    display_.drawLine(mouthLeft + mouthWidth - 1, mouthTop + mouthHeight - 1, mouthLeft + mouthWidth - 6,
                      mouthTop + mouthHeight + 1);
  } else if (mouthCurve < -0.25f) {
    display_.drawLine(mouthLeft, mouthTop + 1, mouthLeft + 6, mouthTop - 2);
    display_.drawLine(mouthLeft + mouthWidth - 1, mouthTop + 1, mouthLeft + mouthWidth - 6, mouthTop - 2);
  } else {
    display_.drawLine(mouthLeft, mouthTop + mouthHeight, mouthLeft + mouthWidth, mouthTop + mouthHeight);
  }

  if (interaction > 0.6f) {
    int16_t sparkY = centerY - 26;
    display_.drawPixel(centerX - eyeSpacing - 6, sparkY);
    display_.drawPixel(centerX + eyeSpacing + 6, sparkY + 1);
    display_.drawPixel(centerX - 2, sparkY + 4);
  }
}
void DisplayManager::drawMenuLayer(const MenuListView& menu) {
  display_.setFont(u8g2_font_6x12_tf);
  if (menu.title != nullptr) {
    int16_t width = display_.getStrWidth(menu.title);
    display_.drawStr((128 - width) / 2, 12, menu.title);
  }
  display_.drawLine(0, 16, 128, 16);

  uint8_t visible = std::min<uint8_t>(menu.entryCount, MenuListView::kMaxVisible);
  for (uint8_t i = 0; i < visible; ++i) {
    uint8_t index = menu.topIndex + i;
    if (index >= menu.totalCount) {
      break;
    }
    const char* label = menu.items[i] != nullptr ? menu.items[i] : "";
    bool selected = (index == menu.selectedIndex);
    int16_t y = 30 + i * 12;
    if (selected) {
      display_.drawBox(0, y - 11, 128, 12);
      display_.setDrawColor(0);
    }
    display_.drawStr(6, y - 1, label);
    if (selected) {
      display_.setDrawColor(1);
    }
  }

  if (menu.topIndex > 0) {
    display_.drawTriangle(124, 20, 126, 16, 122, 16);
  }
  if (menu.topIndex + menu.entryCount < menu.totalCount) {
    display_.drawTriangle(124, 58, 122, 62, 126, 62);
  }
}

void DisplayManager::drawInfoLayer(const sensing::EnvironmentReadings& environment, const SystemStatusView& status) {
  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(4, 12, "Plant insights");
  display_.drawLine(0, 16, 128, 16);

  display_.setFont(u8g2_font_5x8_tf);
  int16_t y = 26;
  char buffer[32];

  if (status.fetchInProgress) {
    display_.drawStr(4, y, "Fetching profile...");
    y += 10;
    if (status.profileStatus.length() > 0) {
      display_.drawStr(4, y, status.profileStatus.c_str());
      y += 10;
    }
  } else if (status.profile != nullptr && status.profile->valid) {
    const plant::PlantProfile& profile = *status.profile;
    display_.drawStr(4, y, profile.speciesCommonName.length() ? profile.speciesCommonName.c_str() : "Unnamed companion");
    y += 10;
    if (profile.summary.length() > 0) {
      display_.drawStr(4, y, profile.summary.c_str());
      y += 10;
    }
    std::snprintf(buffer, sizeof(buffer), "Soil %2.0f-%2.0f%%", profile.soilTargetMinPct, profile.soilTargetMaxPct);
    display_.drawStr(4, y, buffer);
    y += 10;
    std::snprintf(buffer, sizeof(buffer), "Light %2.0f-%2.0f%%", profile.lightTargetMinPct, profile.lightTargetMaxPct);
    display_.drawStr(4, y, buffer);
    y += 10;
  } else {
    display_.drawStr(4, y, "No profile yet. Use menu or app.");
    y += 10;
    if (status.wifiStatus.length() > 0) {
      display_.drawStr(4, y, status.wifiStatus.c_str());
      y += 10;
    }
  }

  y = std::max<int16_t>(y, 38);
  display_.drawLine(0, y, 128, y);
  y += 10;

  std::snprintf(buffer, sizeof(buffer), "Soil %s %2.0f%%",
                environment.soilValid ? "" : "(?)", environment.soilMoisturePct);
  display_.drawStr(4, y, buffer);
  std::snprintf(buffer, sizeof(buffer), "Light %s %2.0f%%",
                environment.lightValid ? "" : "(?)", environment.lightPct);
  display_.drawStr(4, y + 10, buffer);
  if (environment.climateValid) {
    std::snprintf(buffer, sizeof(buffer), "Temp %2.1fC  Hum %2.0f%%", environment.temperatureC, environment.humidityPct);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Temp --.-C  Hum --.-%%");
  }
  display_.drawStr(4, y + 20, buffer);

  if (status.profile != nullptr && status.profile->valid) {
    const plant::PlantProfile& profile = *status.profile;
    int16_t tipY = y + 32;
    for (uint8_t i = 0; i < 3; ++i) {
      if (profile.tips[i].length() == 0) continue;
      display_.drawStr(4, tipY, profile.tips[i].c_str());
      tipY += 10;
      if (tipY > 62) break;
    }
  } else if (status.profileStatus.length() > 0) {
    display_.drawStr(4, y + 32, status.profileStatus.c_str());
  }
}

void DisplayManager::drawDebugLayer(const sensing::EnvironmentReadings& environment, const SystemStatusView& status) {
  display_.setFont(u8g2_font_5x8_tf);
  display_.drawStr(4, 12, "Debug raw values");
  display_.drawLine(0, 16, 128, 16);

  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "Soil raw : %4u", environment.soilRaw);
  display_.drawStr(4, 28, buffer);
  std::snprintf(buffer, sizeof(buffer), "Light raw: %4u", environment.lightRaw);
  display_.drawStr(4, 38, buffer);
  std::snprintf(buffer, sizeof(buffer), "Temp : %s", environment.climateValid ? "valid" : "n/a");
  display_.drawStr(4, 48, buffer);
  std::snprintf(buffer, sizeof(buffer), "Hum  : %s", environment.climateValid ? "valid" : "n/a");
  display_.drawStr(4, 58, buffer);

  if (status.wifiStatus.length() > 0) {
    display_.drawStr(74, 28, status.wifiStatus.c_str());
  }
}

void DisplayManager::drawFooter(PageId page, uint8_t pageIndex, uint8_t pageCount, bool menuVisible) {
  display_.setFont(u8g2_font_5x8_tf);
  display_.drawBox(0, 63, 128, 1);
  if (menuVisible) {
    display_.drawTriangle(12, 58, 20, 62, 20, 54);
    display_.drawTriangle(116, 54, 116, 62, 124, 58);
    display_.drawCircle(64, 58, 4);
    display_.setDrawColor(0);
    display_.drawCircle(64, 58, 2);
    display_.setDrawColor(1);
    return;
  }

  uint8_t pageIdx = std::min<uint8_t>(pageIndex, pageCount > 0 ? pageCount - 1 : 0);
  if (static_cast<uint8_t>(page) < kPageTitleCount) {
    display_.drawStr(2, 62, kPageTitles[static_cast<uint8_t>(page)]);
  }

  const char* hint = "OK=Menu";
  int16_t hintWidth = display_.getStrWidth(hint);
  display_.drawStr((128 - hintWidth) / 2, 62, hint);

  if (pageCount == 0) {
    return;
  }

  char counter[12];
  std::snprintf(counter, sizeof(counter), "%u/%u", static_cast<unsigned>(pageIdx + 1), static_cast<unsigned>(pageCount));
  int16_t width = display_.getStrWidth(counter);
  display_.drawStr(128 - width - 2, 62, counter);
}

}  // namespace display

