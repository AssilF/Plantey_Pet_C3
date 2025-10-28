#include "web_service.h"

#include <WiFi.h>

#include "plant_profile.h"
#include "logging.h"

namespace web {

namespace {
void addCorsHeaders(WebServer& server) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

ui::CalibrationTarget parseCalibrationTarget(const String& name) {
  if (name.equalsIgnoreCase("soilDry")) return ui::CalibrationTarget::SoilDry;
  if (name.equalsIgnoreCase("soilWet")) return ui::CalibrationTarget::SoilWet;
  if (name.equalsIgnoreCase("lightDark")) return ui::CalibrationTarget::LightDark;
  if (name.equalsIgnoreCase("lightBright")) return ui::CalibrationTarget::LightBright;
  return ui::CalibrationTarget::None;
}
}  // namespace

constexpr const char* kLogTagWeb = "web";

void WebService::begin() {
  server_.on("/", HTTP_GET, std::bind(&WebService::handleRoot, this));
  server_.on("/api/status", HTTP_GET, std::bind(&WebService::handleStatus, this));
  server_.on("/api/plant", HTTP_POST, std::bind(&WebService::handlePlantPost, this));
  server_.on("/api/calibrate", HTTP_POST, std::bind(&WebService::handleCalibratePost, this));
  server_.on("/api/display", HTTP_POST, std::bind(&WebService::handleDisplayPost, this));
  server_.on("/api/profile/reset", HTTP_POST, std::bind(&WebService::handleProfileReset, this));
  server_.onNotFound(std::bind(&WebService::handleNotFound, this));
  server_.on("/api/status", HTTP_OPTIONS, std::bind(&WebService::handleOptions, this));
  server_.on("/api/plant", HTTP_OPTIONS, std::bind(&WebService::handleOptions, this));
  server_.on("/api/calibrate", HTTP_OPTIONS, std::bind(&WebService::handleOptions, this));
  server_.on("/api/display", HTTP_OPTIONS, std::bind(&WebService::handleOptions, this));
  server_.on("/api/profile/reset", HTTP_OPTIONS, std::bind(&WebService::handleOptions, this));
  server_.begin();
  LOG_INFO(kLogTagWeb, "Web service started on port 80");
}

void WebService::loop() {
  server_.handleClient();
}

void WebService::setPresetList(const char* const* presets, uint8_t count) {
  presets_ = presets;
  presetCount_ = count;
}

void WebService::updateState(const sensing::EnvironmentReadings& env,
                             const display::SystemStatusView& status,
                             const String& speciesQuery,
                             bool fetchInProgress,
                             uint8_t presetIndex,
                             uint8_t presetCount) {
  lastEnv_ = env;
  lastStatus_ = status;
  speciesQuery_ = speciesQuery;
  fetchInProgress_ = fetchInProgress;
  presetIndex_ = presetIndex;
  presetCount_ = presetCount;
}

void WebService::handleRoot() {
  addCorsHeaders(server_);
  server_.send(200, "text/plain", "PlanteyPetC3 Web API. See /api/status.");
  LOG_DEBUG(kLogTagWeb, "Handled GET /");
}

void WebService::handleStatus() {
  StaticJsonDocument<1536> doc;

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["status"] = lastStatus_.wifiStatus;
  bool staConnected = network_ != nullptr && network_->isConnected();
  wifi["staConnected"] = staConnected;
  if (staConnected) {
    wifi["staIp"] = WiFi.localIP().toString();
  }
  wifi["apIp"] = WiFi.softAPIP().toString();
  wifi["apSsid"] = WiFi.softAPSSID();

  JsonObject plant = doc.createNestedObject("plant");
  plant["speciesQuery"] = speciesQuery_;
  plant["profileStatus"] = lastStatus_.profileStatus;
  plant["fetchInProgress"] = fetchInProgress_;
  plant["presetIndex"] = presetIndex_;
  plant["presetCount"] = presetCount_;
  plant["hasProfile"] = lastStatus_.profile != nullptr && lastStatus_.profile->valid;
  if (lastStatus_.profile != nullptr && lastStatus_.profile->valid) {
    const plant::PlantProfile& profile = *lastStatus_.profile;
    plant["speciesCommonName"] = profile.speciesCommonName;
    plant["speciesLatinName"] = profile.speciesLatinName;
    plant["soilMin"] = profile.soilTargetMinPct;
    plant["soilMax"] = profile.soilTargetMaxPct;
    plant["lightMin"] = profile.lightTargetMinPct;
    plant["lightMax"] = profile.lightTargetMaxPct;
    plant["comfortTempMin"] = profile.comfortTempMinC;
    plant["comfortTempMax"] = profile.comfortTempMaxC;
    plant["wateringIntervalHours"] = profile.wateringIntervalHours;
  }
  if (presets_ != nullptr && presetCount_ > 0) {
    JsonArray presets = plant.createNestedArray("presets");
    for (uint8_t i = 0; i < presetCount_; ++i) {
      presets.add(String(presets_[i]));
    }
  }

  JsonObject env = doc.createNestedObject("environment");
  env["soilValid"] = lastEnv_.soilValid;
  env["soilPct"] = lastEnv_.soilMoisturePct;
  env["lightValid"] = lastEnv_.lightValid;
  env["lightPct"] = lastEnv_.lightPct;
  env["temperatureValid"] = lastEnv_.climateValid;
  env["temperatureC"] = lastEnv_.temperatureC;
  env["humidityPct"] = lastEnv_.humidityPct;

  sendJsonDocument(doc);
  LOG_DEBUG(kLogTagWeb, "Handled GET /api/status");
}

void WebService::handlePlantPost() {
  StaticJsonDocument<1024> doc;
  if (!parseJsonPayload(doc)) {
    return;
  }

  bool fetchRequested = false;
  bool nextPreset = false;

  if (doc.containsKey("species") && handlers_.setSpecies != nullptr) {
    String species = doc["species"].as<String>();
    handlers_.setSpecies(handlers_.context, species);
    LOG_INFO(kLogTagWeb, "Set species via API to '%s'", species.c_str());
  }

  if (doc.containsKey("nextPreset")) {
    nextPreset = doc["nextPreset"].as<bool>();
  }
  if (doc.containsKey("presetDelta")) {
    int delta = doc["presetDelta"].as<int>();
    if (delta > 0) {
      nextPreset = true;
    }
  }

  if ((doc.containsKey("fetch") && doc["fetch"].as<bool>()) || doc.containsKey("species") || nextPreset) {
    fetchRequested = true;
  }

  if (fetchRequested && handlers_.queueProfileFetch != nullptr) {
    handlers_.queueProfileFetch(handlers_.context, nextPreset);
    LOG_INFO(kLogTagWeb, "Queued profile fetch (nextPreset=%s)", nextPreset ? "true" : "false");
  }

  StaticJsonDocument<256> response;
  response["queued"] = fetchRequested;
  response["nextPreset"] = nextPreset;
  sendJsonDocument(response);
  LOG_DEBUG(kLogTagWeb, "Handled POST /api/plant");
}

void WebService::handleCalibratePost() {
  if (handlers_.queueCalibration == nullptr) {
    sendError(503, F("Calibration handler unavailable"));
    LOG_WARN(kLogTagWeb, "Calibration handler unavailable");
    return;
  }
  StaticJsonDocument<256> doc;
  if (!parseJsonPayload(doc)) {
    return;
  }
  if (!doc.containsKey("target")) {
    sendError(400, F("Missing target"));
    return;
  }
  String target = doc["target"].as<String>();
  ui::CalibrationTarget calTarget = parseCalibrationTarget(target);
  if (calTarget == ui::CalibrationTarget::None) {
    sendError(400, F("Unknown calibration target"));
    LOG_WARN(kLogTagWeb, "Unknown calibration target '%s'", target.c_str());
    return;
  }
  handlers_.queueCalibration(handlers_.context, calTarget);
  StaticJsonDocument<128> response;
  response["accepted"] = true;
  response["target"] = target;
  sendJsonDocument(response);
  LOG_INFO(kLogTagWeb, "Handled POST /api/calibrate target=%s", target.c_str());
}

void WebService::handleDisplayPost() {
  if (handlers_.adjustContrast == nullptr && handlers_.playDemo == nullptr) {
    sendError(503, F("Display handlers unavailable"));
    LOG_WARN(kLogTagWeb, "Display handlers unavailable");
    return;
  }
  StaticJsonDocument<256> doc;
  if (!parseJsonPayload(doc)) {
    return;
  }

  bool acted = false;
  String note;

  if (doc.containsKey("contrastDelta")) {
    int delta = doc["contrastDelta"].as<int>();
    if (handlers_.adjustContrast != nullptr) {
      handlers_.adjustContrast(handlers_.context, static_cast<int8_t>(delta));
    }
    note = "Contrast control not supported on SH1106 OLED";
    LOG_WARN(kLogTagWeb, "Contrast adjustment (%d) requested but unsupported", delta);
  }

  if (doc.containsKey("playDemo") && doc["playDemo"].as<bool>() && handlers_.playDemo != nullptr) {
    handlers_.playDemo(handlers_.context);
    acted = true;
    LOG_INFO(kLogTagWeb, "Triggered demo chord via API");
  }

  StaticJsonDocument<128> response;
  response["accepted"] = acted;
  if (note.length() > 0) {
    response["note"] = note;
  }
  sendJsonDocument(response);
  LOG_DEBUG(kLogTagWeb, "Handled POST /api/display");
}

void WebService::handleProfileReset() {
  if (handlers_.resetProfile == nullptr) {
    sendError(503, F("Reset handler unavailable"));
    LOG_WARN(kLogTagWeb, "Profile reset handler unavailable");
    return;
  }
  handlers_.resetProfile(handlers_.context);
  StaticJsonDocument<128> response;
  response["reset"] = true;
  sendJsonDocument(response);
  LOG_WARN(kLogTagWeb, "Handled POST /api/profile/reset");
}

void WebService::handleNotFound() {
  sendError(404, F("Not found"));
  LOG_DEBUG(kLogTagWeb, "Unhandled request -> 404");
}

void WebService::handleOptions() {
  addCorsHeaders(server_);
  server_.send(204);
  LOG_DEBUG(kLogTagWeb, "Handled CORS preflight");
}

bool WebService::sendJsonResponse(const String& json, int code) {
  addCorsHeaders(server_);
  server_.send(code, "application/json", json);
  return true;
}

bool WebService::sendJsonDocument(const JsonDocument& doc, int code) {
  String payload;
  serializeJson(doc, payload);
  return sendJsonResponse(payload, code);
}

void WebService::sendError(int code, const __FlashStringHelper* message) {
  StaticJsonDocument<256> doc;
  doc["error"] = String(message);
  sendJsonDocument(doc, code);
}

void WebService::sendError(int code, const String& message) {
  StaticJsonDocument<256> doc;
  doc["error"] = message;
  sendJsonDocument(doc, code);
}

WebService service;

}  // namespace web


