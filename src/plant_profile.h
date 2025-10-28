#pragma once

#include <Arduino.h>

namespace brain {
class ExpressionLogic;
}  // namespace brain

namespace plant {

struct PlantProfile {
  String speciesCommonName;
  String speciesLatinName;
  String speciesQuery;
  String summary;
  float soilDryThreshold = 35.0f;
  float soilSoggyThreshold = 85.0f;
  float soilTargetMinPct = 45.0f;
  float soilTargetMaxPct = 65.0f;
  float lightLowThreshold = 25.0f;
  float lightHighThreshold = 90.0f;
  float lightTargetMinPct = 40.0f;
  float lightTargetMaxPct = 80.0f;
  float comfortTempMinC = 18.0f;
  float comfortTempMaxC = 28.0f;
  float humidityMinPct = 35.0f;
  float humidityMaxPct = 70.0f;
  uint16_t wateringIntervalHours = 72;
  String wateringStrategy;
  String lightingStrategy;
  String feedingStrategy;
  String tips[3];
  uint32_t generatedAtEpoch = 0;
  bool valid = false;
};

// Deserialize a PlantProfile from a JSON string produced by the AI helper.
bool DecodeProfileFromJson(const String& json, PlantProfile& profile, String& error);

// Serialize a PlantProfile to a compact JSON string for storage.
String EncodeProfileToJson(const PlantProfile& profile);

class PlantProfileManager {
 public:
  void begin();

  bool loadFromStorage();
  bool saveToStorage() const;
  void clearProfile();

  void setProfile(const PlantProfile& profile);
  bool hasProfile() const { return profile_.valid; }
  const PlantProfile& profile() const { return profile_; }

  void applyTo(brain::ExpressionLogic& logic) const;

  String status() const { return status_; }
  void setStatus(const String& status) { status_ = status; }

 private:
  PlantProfile profile_;
  String status_ = "Profile idle";
};

}  // namespace plant
