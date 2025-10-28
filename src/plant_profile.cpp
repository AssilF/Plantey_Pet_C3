#include "plant_profile.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <cmath>

#include "expression_logic.h"

namespace plant {
namespace {
constexpr size_t kStorageDocCapacity = 2048;
constexpr const char* kPrefsNamespace = "plantey";
constexpr const char* kPrefsKeyProfile = "profile";

void applyStringOrClear(String& target, const String& value) {
  if (value.length() == 0) {
    target = "";
  } else {
    target = value;
  }
}

float readFloatOrDefault(JsonVariantConst variant, float fallback) {
  if (variant.isNull()) {
    return fallback;
  }
  return variant.as<float>();
}

uint16_t readUint16OrDefault(JsonVariantConst variant, uint16_t fallback) {
  if (variant.isNull()) {
    return fallback;
  }
  return static_cast<uint16_t>(variant.as<uint32_t>());
}

}  // namespace

bool DecodeProfileFromJson(const String& json, PlantProfile& profile, String& error) {
  StaticJsonDocument<kStorageDocCapacity> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    error = String("JSON parse failed: ") + err.f_str();
    return false;
  }
  JsonObject root = doc.as<JsonObject>();
  if (root.isNull()) {
    error = "Profile JSON missing root object";
    return false;
  }

  applyStringOrClear(profile.speciesCommonName, root["speciesCommonName"].as<String>());
  applyStringOrClear(profile.speciesLatinName, root["speciesLatinName"].as<String>());
  applyStringOrClear(profile.speciesQuery, root["speciesQuery"].as<String>());
  applyStringOrClear(profile.summary, root["summary"].as<String>());
  profile.soilDryThreshold = readFloatOrDefault(root["soil"]["dryPercent"], profile.soilDryThreshold);
  profile.soilSoggyThreshold = readFloatOrDefault(root["soil"]["soggyPercent"], profile.soilSoggyThreshold);

  JsonArray soilRange = root["soil"]["targetPercentRange"].as<JsonArray>();
  if (!soilRange.isNull() && soilRange.size() >= 2) {
    float low = soilRange[0].as<float>();
    float high = soilRange[1].as<float>();
    if (!std::isnan(low) && !std::isnan(high)) {
      profile.soilTargetMinPct = low;
      profile.soilTargetMaxPct = high;
    }
  } else {
    profile.soilTargetMinPct = profile.soilDryThreshold;
    profile.soilTargetMaxPct = profile.soilSoggyThreshold;
  }

  profile.lightLowThreshold = readFloatOrDefault(root["light"]["lowPercent"], profile.lightLowThreshold);
  profile.lightHighThreshold = readFloatOrDefault(root["light"]["highPercent"], profile.lightHighThreshold);

  JsonArray lightRange = root["light"]["targetPercentRange"].as<JsonArray>();
  if (!lightRange.isNull() && lightRange.size() >= 2) {
    float low = lightRange[0].as<float>();
    float high = lightRange[1].as<float>();
    if (!std::isnan(low) && !std::isnan(high)) {
      profile.lightTargetMinPct = low;
      profile.lightTargetMaxPct = high;
    }
  } else {
    profile.lightTargetMinPct = profile.lightLowThreshold;
    profile.lightTargetMaxPct = profile.lightHighThreshold;
  }
  profile.comfortTempMinC = readFloatOrDefault(root["temperatureC"]["minComfort"], profile.comfortTempMinC);
  profile.comfortTempMaxC = readFloatOrDefault(root["temperatureC"]["maxComfort"], profile.comfortTempMaxC);
  profile.humidityMinPct = readFloatOrDefault(root["humidityPct"]["min"], profile.humidityMinPct);
  profile.humidityMaxPct = readFloatOrDefault(root["humidityPct"]["max"], profile.humidityMaxPct);
  profile.wateringIntervalHours = readUint16OrDefault(root["wateringIntervalHours"], profile.wateringIntervalHours);

  applyStringOrClear(profile.wateringStrategy, root["wateringStrategy"].as<String>());
  applyStringOrClear(profile.lightingStrategy, root["lightingStrategy"].as<String>());
  applyStringOrClear(profile.feedingStrategy, root["feedingStrategy"].as<String>());

  JsonArray tips = root["careTips"].as<JsonArray>();
  if (!tips.isNull()) {
    for (size_t i = 0; i < tips.size() && i < 3; ++i) {
      profile.tips[i] = tips[i].as<String>();
    }
  }

  profile.generatedAtEpoch = root["generatedAtEpoch"].as<uint32_t>();
  profile.valid = true;
  error = "";
  return true;
}

String EncodeProfileToJson(const PlantProfile& profile) {
  StaticJsonDocument<kStorageDocCapacity> doc;
  JsonObject root = doc.to<JsonObject>();

  root["speciesCommonName"] = profile.speciesCommonName;
  root["speciesLatinName"] = profile.speciesLatinName;
  root["speciesQuery"] = profile.speciesQuery;
  root["summary"] = profile.summary;
  JsonObject soil = root.createNestedObject("soil");
  soil["dryPercent"] = profile.soilDryThreshold;
  soil["soggyPercent"] = profile.soilSoggyThreshold;
  JsonArray soilRange = soil.createNestedArray("targetPercentRange");
  soilRange.add(profile.soilTargetMinPct);
  soilRange.add(profile.soilTargetMaxPct);

  JsonObject light = root.createNestedObject("light");
  light["lowPercent"] = profile.lightLowThreshold;
  light["highPercent"] = profile.lightHighThreshold;
  JsonArray lightRange = light.createNestedArray("targetPercentRange");
  lightRange.add(profile.lightTargetMinPct);
  lightRange.add(profile.lightTargetMaxPct);

  JsonObject temperature = root.createNestedObject("temperatureC");
  temperature["minComfort"] = profile.comfortTempMinC;
  temperature["maxComfort"] = profile.comfortTempMaxC;

  JsonObject humidity = root.createNestedObject("humidityPct");
  humidity["min"] = profile.humidityMinPct;
  humidity["max"] = profile.humidityMaxPct;

  root["wateringIntervalHours"] = profile.wateringIntervalHours;
  root["wateringStrategy"] = profile.wateringStrategy;
  root["lightingStrategy"] = profile.lightingStrategy;
  root["feedingStrategy"] = profile.feedingStrategy;
  root["generatedAtEpoch"] = profile.generatedAtEpoch;

  JsonArray tips = root.createNestedArray("careTips");
  for (size_t i = 0; i < 3; ++i) {
    if (profile.tips[i].length() > 0) {
      tips.add(profile.tips[i]);
    }
  }

  String output;
  serializeJson(doc, output);
  return output;
}

void PlantProfileManager::begin() {
  loadFromStorage();
}

bool PlantProfileManager::loadFromStorage() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    status_ = "Prefs open failed";
    return false;
  }
  String stored = prefs.getString(kPrefsKeyProfile, "");
  prefs.end();

  if (stored.length() == 0) {
    status_ = "No stored profile";
    profile_.valid = false;
    return false;
  }

  String error;
  if (!DecodeProfileFromJson(stored, profile_, error)) {
    status_ = "Profile parse error: " + error;
    profile_.valid = false;
    return false;
  }

  status_ = "Profile loaded";
  return true;
}

bool PlantProfileManager::saveToStorage() const {
  if (!profile_.valid) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }
  String encoded = EncodeProfileToJson(profile_);
  bool ok = prefs.putString(kPrefsKeyProfile, encoded);
  prefs.end();
  return ok;
}

void PlantProfileManager::clearProfile() {
  Preferences prefs;
  if (prefs.begin(kPrefsNamespace, false)) {
    prefs.remove(kPrefsKeyProfile);
    prefs.end();
  }
  profile_ = PlantProfile();
  profile_.valid = false;
  status_ = "Profile cleared";
}

void PlantProfileManager::setProfile(const PlantProfile& profile) {
  profile_ = profile;
  profile_.valid = profile.valid;
  if (!profile_.valid) {
    status_ = "Profile invalid";
  } else {
    status_ = "Profile ready";
  }
}

void PlantProfileManager::applyTo(brain::ExpressionLogic& logic) const {
  if (!profile_.valid) {
    return;
  }
  logic.setSoilThresholds(profile_.soilDryThreshold, profile_.soilSoggyThreshold);
  logic.setLightThresholds(profile_.lightLowThreshold, profile_.lightHighThreshold);
  logic.setTemperatureComfortRange(profile_.comfortTempMinC, profile_.comfortTempMaxC);
}

}  // namespace plant
