#include "display_manager.h"

#include <Wire.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace display {
namespace {

constexpr const char* kPageTitles[] = {"Mood", "Stats", "Tips", "Debug"};

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

}  // namespace

void DisplayManager::begin() {
  if (started_) {
    return;
  }
  Wire.begin(hw::PIN_I2C_SDA, hw::PIN_I2C_SCL);
  display_.begin();
  display_.setContrast(contrast_);
  started_ = true;
}

void DisplayManager::render(const FaceExpressionView& face,
                            const sensing::EnvironmentReadings& environment,
                            PageId page,
                            uint8_t pageIndex,
                            uint8_t pageCount,
                            bool blinkFrame) {
  if (!started_) {
    begin();
  }

  display_.clearBuffer();
  switch (page) {
    case PageId::Mood:
      drawFaceLayer(face, blinkFrame);
      break;
    case PageId::Stats:
      drawStatsLayer(environment);
      break;
    case PageId::Tips:
      drawTipsLayer(face, environment);
      break;
    case PageId::Debug:
      drawDebugLayer(environment);
      break;
  }
  drawFooter(page, pageIndex, pageCount);
  display_.sendBuffer();
}

void DisplayManager::drawSplash(const char* line1, const char* line2) {
  if (!started_) {
    begin();
  }
  display_.clearBuffer();
  display_.setFont(u8g2_font_8x13_tf);
  int16_t width = display_.getStrWidth(line1);
  display_.drawStr((128 - width) / 2, 26, line1);
  if (line2 != nullptr && line2[0] != '\0') {
    display_.setFont(u8g2_font_6x12_tf);
    width = display_.getStrWidth(line2);
    display_.drawStr((128 - width) / 2, 46, line2);
  }
  display_.sendBuffer();
}

void DisplayManager::setContrast(uint8_t level) {
  contrast_ = level;
  if (started_) {
    display_.setContrast(level);
  }
}

void DisplayManager::drawFaceLayer(const FaceExpressionView& face, bool blinkFrame) {
  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(4, 12, face.title);
  if (face.subtitle != nullptr && face.subtitle[0] != '\0') {
    display_.setFont(u8g2_font_5x8_tf);
    display_.drawStr(4, 22, face.subtitle);
  }

  const int16_t centerX = 64;
  const int16_t centerY = 40;
  const int16_t faceRadius = 28;
  display_.drawCircle(centerX, centerY, faceRadius);

  const int16_t eyeOffsetX = 18;
  const int16_t eyeY = centerY - 8;
  const int16_t eyeRadius = 6;

  auto drawEye = [&](int16_t x) {
    if (blinkFrame) {
      display_.drawLine(x - eyeRadius, eyeY, x + eyeRadius, eyeY);
      return;
    }
    if (face.eyelidLevel > 0) {
      int16_t lidHeight = clampInt(face.eyelidLevel * 2, 1, eyeRadius);
      display_.drawDisc(x, eyeY, eyeRadius);
      display_.setDrawColor(0);
      display_.drawBox(x - eyeRadius, eyeY - eyeRadius, eyeRadius * 2, lidHeight);
      display_.setDrawColor(1);
    } else {
      display_.drawDisc(x, eyeY, eyeRadius);
      display_.setDrawColor(0);
      display_.drawDisc(x, eyeY, eyeRadius - 2);
      display_.setDrawColor(1);
    }

    if (face.eyesHappy) {
      display_.drawLine(x - eyeRadius + 1, eyeY + 3, x - 1, eyeY + 1);
      display_.drawLine(x + 1, eyeY + 1, x + eyeRadius - 1, eyeY + 3);
    } else {
      display_.drawLine(x - eyeRadius + 1, eyeY - 3, x - 1, eyeY - 1);
      display_.drawLine(x + 1, eyeY - 1, x + eyeRadius - 1, eyeY - 3);
    }

    if (face.showSparkle) {
      display_.drawPixel(x - 2, eyeY - 2);
      display_.drawPixel(x - 3, eyeY - 1);
    }
  };

  drawEye(centerX - eyeOffsetX);
  drawEye(centerX + eyeOffsetX);

  // Mouth rendering using simple arcs.
  const int16_t mouthY = centerY + 14;
  const uint8_t mouthRadius = 18;
  int8_t mouthLevel = clampInt(face.mouthLevel, -4, 4);
  if (mouthLevel >= 0) {
    uint8_t startAngle = static_cast<uint8_t>(200 - mouthLevel * 10);
    uint8_t endAngle = static_cast<uint8_t>(340 + mouthLevel * 6);
    display_.drawArc(centerX, mouthY, mouthRadius, startAngle, endAngle);
    display_.drawArc(centerX, mouthY + 1, mouthRadius, startAngle, endAngle);
  } else {
    uint8_t startAngle = static_cast<uint8_t>(20 - mouthLevel * 8);
    uint8_t endAngle = static_cast<uint8_t>(160 + mouthLevel * 6);
    display_.drawArc(centerX, mouthY + 10, mouthRadius, startAngle, endAngle);
    display_.drawArc(centerX, mouthY + 9, mouthRadius, startAngle, endAngle);
  }

  if (face.showHeart) {
    display_.drawCircle(centerX + 26, centerY - 18, 4);
    display_.drawCircle(centerX + 21, centerY - 18, 4);
    display_.drawTriangle(centerX + 17, centerY - 16, centerX + 30, centerY - 16, centerX + 24, centerY - 4);
  }

  if (face.showTear) {
    display_.drawTriangle(centerX - eyeOffsetX - 4, eyeY + eyeRadius + 2, centerX - eyeOffsetX + 4,
                          eyeY + eyeRadius + 2, centerX - eyeOffsetX, eyeY + eyeRadius + 10);
  }

  if (face.showSweat) {
    display_.drawTriangle(centerX + eyeOffsetX + 8, eyeY - eyeRadius - 6, centerX + eyeOffsetX + 12,
                          eyeY - eyeRadius - 2, centerX + eyeOffsetX + 6, eyeY - eyeRadius + 1);
  }
}

void DisplayManager::drawStatsLayer(const sensing::EnvironmentReadings& environment) {
  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(4, 14, "Env stats");
  display_.drawLine(0, 18, 128, 18);

  display_.setFont(u8g2_font_5x8_tf);
  char buffer[18];

  if (environment.climateValid) {
    std::snprintf(buffer, sizeof(buffer), "Temp : %2.1fC", environment.temperatureC);
    display_.drawStr(4, 30, buffer);
    std::snprintf(buffer, sizeof(buffer), "Hum  : %2.1f%%", environment.humidityPct);
    display_.drawStr(4, 42, buffer);
  } else {
    display_.drawStr(4, 30, "Temp : --.-C");
    display_.drawStr(4, 42, "Hum  : --.-%");
  }

  if (environment.soilValid) {
    std::snprintf(buffer, sizeof(buffer), "Soil : %2.0f%%", environment.soilMoisturePct);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Soil : ---");
  }
  display_.drawStr(4, 54, buffer);

  if (environment.lightValid) {
    std::snprintf(buffer, sizeof(buffer), "Light: %2.0f%%", environment.lightPct);
  } else {
    std::snprintf(buffer, sizeof(buffer), "Light: ---");
  }
  display_.drawStr(64, 54, buffer);
}

void DisplayManager::drawTipsLayer(const FaceExpressionView& face, const sensing::EnvironmentReadings& environment) {
  display_.setFont(u8g2_font_6x12_tf);
  display_.drawStr(4, 14, "Care tips");
  display_.drawLine(0, 18, 128, 18);

  display_.setFont(u8g2_font_5x8_tf);
  const char* subtitle = (face.subtitle != nullptr && face.subtitle[0] != '\0') ? face.subtitle : "Monitor the plant";
  display_.drawStr(4, 30, subtitle);

  if (environment.soilValid) {
    if (environment.soilMoisturePct < 30.0f) {
      display_.drawStr(4, 44, "Soil dry -> water soon");
    } else if (environment.soilMoisturePct > 80.0f) {
      display_.drawStr(4, 44, "Soil wet -> pause watering");
    } else {
      display_.drawStr(4, 44, "Soil OK -> keep schedule");
    }
  } else {
    display_.drawStr(4, 44, "Soil sensor offline");
  }

  if (environment.lightValid) {
    if (environment.lightPct < 20.0f) {
      display_.drawStr(4, 56, "Needs more light");
    } else if (environment.lightPct > 90.0f) {
      display_.drawStr(4, 56, "Shade if leaves scorch");
    } else {
      display_.drawStr(4, 56, "Light levels good");
    }
  } else {
    display_.drawStr(4, 56, "Light sensor offline");
  }
}

void DisplayManager::drawDebugLayer(const sensing::EnvironmentReadings& environment) {
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
}

void DisplayManager::drawFooter(PageId page, uint8_t pageIndex, uint8_t pageCount) {
  display_.setFont(u8g2_font_5x8_tf);
  display_.drawBox(0, 63, 128, 1);
  display_.drawStr(2, 62, kPageTitles[static_cast<uint8_t>(page)]);

  if (pageCount == 0) {
    return;
  }

  char buffer[12];
  uint8_t index = std::min<uint8_t>(pageIndex, pageCount - 1);
  std::snprintf(buffer, sizeof(buffer), "%u/%u", index + 1, pageCount);
  int16_t width = display_.getStrWidth(buffer);
  display_.drawStr(128 - width - 2, 62, buffer);
}

}  // namespace display
