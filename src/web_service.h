#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "menu_controller.h"
#include "network_manager.h"
#include "sensors.h"
#include "display_manager.h"

namespace web {

struct CommandHandlers {
  void* context = nullptr;
  void (*setSpecies)(void* ctx, const String& species) = nullptr;
  void (*queueProfileFetch)(void* ctx, bool nextPreset) = nullptr;
  void (*queueCalibration)(void* ctx, ui::CalibrationTarget target) = nullptr;
  void (*adjustContrast)(void* ctx, int8_t delta) = nullptr;
  void (*playDemo)(void* ctx) = nullptr;
  void (*resetProfile)(void* ctx) = nullptr;
};

class WebService {
 public:
  void begin();
  void loop();

  void attachNetworkManager(const net::NetworkManager* network) { network_ = network; }
  void setHandlers(const CommandHandlers& handlers) { handlers_ = handlers; }
  void setPresetList(const char* const* presets, uint8_t count);
  void updateState(const sensing::EnvironmentReadings& env,
                   const display::SystemStatusView& status,
                   const String& speciesQuery,
                   bool fetchInProgress,
                   uint8_t presetIndex,
                   uint8_t presetCount);

 private:
  void handleRoot();
  void handleStatus();
  void handlePlantPost();
  void handleCalibratePost();
  void handleDisplayPost();
  void handleProfileReset();
  void handleNotFound();
  void handleOptions();

  bool sendJsonResponse(const String& json, int code = 200);
  bool sendJsonDocument(const JsonDocument& doc, int code = 200);
  template <size_t Capacity>
  bool parseJsonPayload(StaticJsonDocument<Capacity>& doc);
  void sendError(int code, const __FlashStringHelper* message);
  void sendError(int code, const String& message);

  WebServer server_{80};
  CommandHandlers handlers_;
  const net::NetworkManager* network_ = nullptr;

  sensing::EnvironmentReadings lastEnv_{};
  display::SystemStatusView lastStatus_;
  String speciesQuery_;
  bool fetchInProgress_ = false;
  uint8_t presetIndex_ = 0;
  uint8_t presetCount_ = 0;
  const char* const* presets_ = nullptr;
};

extern WebService service;

}  // namespace web

template <size_t Capacity>
bool web::WebService::parseJsonPayload(StaticJsonDocument<Capacity>& doc) {
  if (!server_.hasArg("plain")) {
    sendError(400, F("Missing body"));
    return false;
  }
  DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    sendError(400, String(F("JSON parse error: ")) + err.c_str());
    return false;
  }
  return true;
}
