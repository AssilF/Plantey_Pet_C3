#include <Arduino.h>
#include <Wire.h>
#include <cstdio>
#include <esp_random.h>

#include "ai_client.h"
#include "audio_engine.h"
#include "buttons.h"
#include "display_manager.h"
#include "expression_logic.h"
#include "hardware_config.h"
#include "menu_controller.h"
#include "network_manager.h"
#include "plant_profile.h"
#include "sensors.h"
#include "web_service.h"
#include "logging.h"

namespace {
constexpr uint32_t kSensorIntervalMs = 1500;
constexpr uint32_t kDisplayIntervalMs = 100;

constexpr display::PageId kScreenOrder[] = {
    display::PageId::Mood, display::PageId::Info, display::PageId::Debug};
constexpr uint8_t kScreenCount = sizeof(kScreenOrder) / sizeof(display::PageId);

constexpr const char* kPresetSpecies[] = {
    "Golden pothos", "Snake plant", "Peace lily", "Aloe vera", "Boston fern", "Spider plant"};
constexpr uint8_t kPresetCount = sizeof(kPresetSpecies) / sizeof(kPresetSpecies[0]);

sensing::SensorSuite sensors;
audio::AudioEngine audioEngine;
display::DisplayManager displayManager;
input::ButtonInput buttons(hw::PIN_BUTTON_LEFT, hw::PIN_BUTTON_RIGHT, /*activeLow=*/true,
                           hw::BUTTON_DEBOUNCE_MS, hw::BUTTON_LONG_PRESS_MS);
brain::ExpressionLogic expressionLogic;
ui::MenuController menuController;
plant::PlantProfileManager profileManager;
ai::PlantKnowledgeClient knowledgeClient;

sensing::EnvironmentReadings lastReadings;
brain::MoodResult currentMood;

uint32_t lastSensorSampleMs = 0;
uint32_t lastDisplayUpdateMs = 0;

bool blinkActive = false;
uint32_t blinkStartMs = 0;
uint32_t nextBlinkAtMs = 0;
constexpr uint32_t kAmbientResumeDelayMs = 6000;
uint32_t ambientResumeAtMs = 0;
uint32_t lastInteractionMs = 0;

uint16_t soilDryCalibration = hw::SOIL_RAW_DRY_DEFAULT;
uint16_t soilWetCalibration = hw::SOIL_RAW_WET_DEFAULT;
uint16_t lightDarkCalibration = hw::LIGHT_RAW_DARK_DEFAULT;
uint16_t lightBrightCalibration = hw::LIGHT_RAW_BRIGHT_DEFAULT;

String profileStatusText = "Use menu (OK=both) to fetch profile";
String wifiStatusText = "WiFi idle";
String speciesQuery = kPresetSpecies[0];
String serialLineBuffer;
uint8_t presetIndex = 0;
bool profileFetchRequested = false;
bool profileFetchInProgress = false;

display::SystemStatusView statusView;
constexpr const char* kLogTagMain = "main";

void scheduleNextBlink(uint32_t nowMs) {
  uint32_t window = hw::BLINK_INTERVAL_MAX_MS - hw::BLINK_INTERVAL_MIN_MS;
  nextBlinkAtMs = nowMs + hw::BLINK_INTERVAL_MIN_MS + random(window);
}

bool updateBlink(uint32_t nowMs) {
  if (blinkActive) {
    if (nowMs - blinkStartMs >= hw::BLINK_DURATION_MS) {
      blinkActive = false;
      scheduleNextBlink(nowMs);
    }
  } else if (nowMs >= nextBlinkAtMs) {
    blinkActive = true;
    blinkStartMs = nowMs;
  }
  return blinkActive;
}

void applyCalibration(ui::CalibrationTarget target) {
  switch (target) {
    case ui::CalibrationTarget::SoilDry:
      soilDryCalibration = lastReadings.soilRaw;
      sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
      Serial.printf("[cal] Soil dry raw=%u\n", soilDryCalibration);
      audioEngine.playTone(523.3f, 220);
      break;
    case ui::CalibrationTarget::SoilWet:
      soilWetCalibration = lastReadings.soilRaw;
      sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
      Serial.printf("[cal] Soil wet raw=%u\n", soilWetCalibration);
      audioEngine.playTone(659.3f, 220);
      break;
    case ui::CalibrationTarget::LightDark:
      lightDarkCalibration = lastReadings.lightRaw;
      sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
      Serial.printf("[cal] Light dark raw=%u\n", lightDarkCalibration);
      audioEngine.playTone(392.0f, 180);
      break;
    case ui::CalibrationTarget::LightBright:
      lightBrightCalibration = lastReadings.lightRaw;
      sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
      Serial.printf("[cal] Light bright raw=%u\n", lightBrightCalibration);
      audioEngine.playTone(784.0f, 180);
      break;
    case ui::CalibrationTarget::None:
    default:
      break;
  }
}

void updateSpeciesQuery(const String& query, bool announce) {
  speciesQuery = query;
  speciesQuery.trim();
  if (speciesQuery.length() == 0) {
    speciesQuery = kPresetSpecies[presetIndex];
  }
  for (uint8_t i = 0; i < kPresetCount; ++i) {
    if (speciesQuery.equalsIgnoreCase(kPresetSpecies[i])) {
      presetIndex = i;
      break;
    }
  }
  if (announce && !profileFetchInProgress) {
    profileStatusText = "Species: " + speciesQuery;
    LOG_INFO(kLogTagMain, "Species query set to '%s'", speciesQuery.c_str());
  }
}

void cyclePreset(int8_t delta) {
  if (delta == 0) {
    return;
  }
  int16_t index = static_cast<int16_t>(presetIndex) + delta;
  while (index < 0) {
    index += kPresetCount;
  }
  while (index >= kPresetCount) {
    index -= kPresetCount;
  }
  presetIndex = static_cast<uint8_t>(index);
  updateSpeciesQuery(String(kPresetSpecies[presetIndex]), true);
  LOG_INFO(kLogTagMain, "Preset index -> %u (%s)", presetIndex, speciesQuery.c_str());
}

void processSerialLine(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }
  if (line.startsWith("plant:")) {
    line.remove(0, 6);
    line.trim();
    if (line.length() == 0) {
      Serial.println(F("[serial] Plant command ignored (empty)"));
      return;
    }
    updateSpeciesQuery(line, true);
    profileFetchRequested = true;
    Serial.printf("[serial] Species set to '%s'\n", speciesQuery.c_str());
    LOG_INFO(kLogTagMain, "Serial command set species to '%s'", speciesQuery.c_str());
  } else if (line.equalsIgnoreCase("profile:fetch")) {
    profileFetchRequested = true;
    profileStatusText = "Fetch requested via serial";
    Serial.println(F("[serial] Triggered profile fetch"));
    LOG_INFO(kLogTagMain, "Serial command queued profile fetch");
  } else if (line.equalsIgnoreCase("profile:clear")) {
    profileManager.clearProfile();
    expressionLogic = brain::ExpressionLogic();
    currentMood = expressionLogic.evaluate(lastReadings);
    profileStatusText = "Profile cleared. Using defaults.";
    Serial.println(F("[serial] Cleared stored profile"));
    LOG_WARN(kLogTagMain, "Profile cleared via serial command");
  } else if (line.equalsIgnoreCase("wifi:status")) {
    Serial.printf("[serial] WiFi status: %s\n", wifiStatusText.c_str());
  } else {
    Serial.printf("[serial] Unknown command: %s\n", line.c_str());
    LOG_WARN(kLogTagMain, "Unknown serial command: %s", line.c_str());
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      if (serialLineBuffer.length() > 0) {
        processSerialLine(serialLineBuffer);
        serialLineBuffer = "";
      }
    } else if (serialLineBuffer.length() < 96) {
      serialLineBuffer += c;
    }
  }
}

uint32_t profileAgeSeconds(uint32_t nowMs) {
  if (!profileManager.hasProfile()) {
    return 0;
  }
  const plant::PlantProfile& profile = profileManager.profile();
  uint32_t nowSec = nowMs / 1000UL;
  uint32_t generated = profile.generatedAtEpoch;
  if (generated == 0 || nowSec < generated) {
    return 0;
  }
  return nowSec - generated;
}

void maybeHandleProfileFetch() {
  if (profileFetchInProgress || !profileFetchRequested) {
    return;
  }

  if (speciesQuery.length() == 0) {
    profileStatusText = "Set plant species (Serial: plant:<name>)";
    profileFetchRequested = false;
    return;
  }

  if (!net::network.ensureConnected()) {
    wifiStatusText = net::network.statusMessage();
    if (wifiStatusText.indexOf("missing") >= 0) {
      profileStatusText = "Configure WiFi in secrets.h";
      profileFetchRequested = false;
      LOG_WARN(kLogTagMain, "WiFi credentials missing; fetch aborted");
    } else {
      profileStatusText = "Waiting for WiFi...";
      LOG_INFO(kLogTagMain, "Waiting for WiFi to fetch profile '%s'", speciesQuery.c_str());
    }
    return;
  }

  profileFetchRequested = false;
  profileFetchInProgress = true;
  profileStatusText = String("Fetching ") + speciesQuery + "...";
  LOG_INFO(kLogTagMain, "Starting profile fetch for '%s'", speciesQuery.c_str());

  plant::PlantProfile profile;
  String error;
  if (knowledgeClient.fetchProfile(speciesQuery, profile, error)) {
    profile.speciesQuery = speciesQuery;
    profile.generatedAtEpoch = millis() / 1000UL;
    profileManager.setProfile(profile);
    if (!profileManager.saveToStorage()) {
      Serial.println(F("[profile] Warning: failed to persist profile"));
      LOG_WARN(kLogTagMain, "Failed to persist fetched profile");
    }
    profileManager.applyTo(expressionLogic);
    currentMood = expressionLogic.evaluate(lastReadings);
    profileStatusText =
        String("Profile loaded: ") +
        (profile.speciesCommonName.length() ? profile.speciesCommonName : speciesQuery);
    if (!audioEngine.isPlaying()) {
      audioEngine.playChord({523.3f, 659.3f, 783.9f}, 650, 10);
    }
    LOG_INFO(kLogTagMain,
             "Profile fetch succeeded: common='%s' soil[%.1f-%.1f] light[%.1f-%.1f]",
             profile.speciesCommonName.c_str(), profile.soilTargetMinPct, profile.soilTargetMaxPct,
             profile.lightTargetMinPct, profile.lightTargetMaxPct);
  } else {
    profileStatusText = "Fetch failed: " + error;
    Serial.printf("[ai] Fetch error: %s\n", error.c_str());
    LOG_WARN(kLogTagMain, "Profile fetch failed: %s", error.c_str());
  }

  profileFetchInProgress = false;
  wifiStatusText = net::network.statusMessage();
}

void refreshStatusView(uint32_t nowMs) {
  wifiStatusText = net::network.statusMessage();
  statusView.profile = profileManager.hasProfile() ? &profileManager.profile() : nullptr;
  statusView.profileStatus = profileStatusText;
  statusView.wifiStatus = wifiStatusText;
  statusView.fetchInProgress = profileFetchInProgress;
  statusView.wifiConnected = net::network.isConnected();
  statusView.profileAgeSeconds = profileAgeSeconds(nowMs);
}

void formatClock(char* buffer, size_t length) {
  if (length < 6) {
    if (length > 0) {
      buffer[0] = '\0';
    }
    return;
  }
  uint32_t totalSeconds = millis() / 1000UL;
  uint32_t minutes = (totalSeconds / 60UL) % 60UL;
  uint32_t hours = (totalSeconds / 3600UL) % 24UL;
  std::snprintf(buffer, length, "%02lu:%02lu", static_cast<unsigned long>(hours),
                static_cast<unsigned long>(minutes));
}

void handleWebSetSpecies(void*, const String& species) {
  updateSpeciesQuery(species, true);
}

void handleWebQueueFetch(void*, bool nextPreset) {
  if (nextPreset) {
    cyclePreset(+1);
  }
  if (!profileFetchInProgress) {
    profileStatusText = nextPreset ? String("Queued fetch (next): ") + speciesQuery
                                   : String("Queued fetch: ") + speciesQuery;
  }
  profileFetchRequested = true;
}

void handleWebCalibration(void*, ui::CalibrationTarget target) {
  if (target != ui::CalibrationTarget::None) {
    applyCalibration(target);
  }
}

void handleWebAdjustContrast(void*, int8_t delta) {
  LOG_WARN(kLogTagMain, "Contrast adjustment (%d) requested but unsupported on SH1106", delta);
}

void handleWebPlayDemo(void*) {
  audioEngine.playChord({523.3f, 659.3f, 783.9f}, 900, 10);
}

void handleWebResetProfile(void*) {
  profileManager.clearProfile();
  expressionLogic = brain::ExpressionLogic();
  profileManager.applyTo(expressionLogic);
  currentMood = expressionLogic.evaluate(lastReadings);
  profileStatusText = "Profile cleared. Using defaults.";
  LOG_WARN(kLogTagMain, "Profile reset via web command");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("PlanteyPetC3 booting...");
  LOG_INFO(kLogTagMain, "Boot start (build debug level %d)", PLANTEY_DEBUG_LEVEL);

  randomSeed(esp_random());
  net::network.begin();
  profileManager.begin();
  web::service.begin();
  web::service.attachNetworkManager(&net::network);
  web::CommandHandlers webHandlers;
  webHandlers.context = nullptr;
  webHandlers.setSpecies = handleWebSetSpecies;
  webHandlers.queueProfileFetch = handleWebQueueFetch;
  webHandlers.queueCalibration = handleWebCalibration;
  webHandlers.adjustContrast = handleWebAdjustContrast;
  webHandlers.playDemo = handleWebPlayDemo;
  webHandlers.resetProfile = handleWebResetProfile;
  web::service.setHandlers(webHandlers);
  web::service.setPresetList(kPresetSpecies, kPresetCount);

  buttons.begin();
  sensors.begin();
  sensors.setSoilCalibration(soilDryCalibration, soilWetCalibration);
  sensors.setLightCalibration(lightDarkCalibration, lightBrightCalibration);
  audioEngine.begin();
  menuController.begin(kScreenOrder, kScreenCount);

  displayManager.begin();
  displayManager.drawSplash("Plantey", "breathing in...");
  LOG_INFO(kLogTagMain, "Display initialized and splash shown");
  audioEngine.playBootSequence();
  LOG_INFO(kLogTagMain, "Boot melody started");
  ambientResumeAtMs = millis() + kAmbientResumeDelayMs;
  delay(600);

  if (profileManager.hasProfile()) {
    const plant::PlantProfile& profile = profileManager.profile();
    profileManager.applyTo(expressionLogic);
    if (profile.speciesQuery.length() > 0) {
      updateSpeciesQuery(profile.speciesQuery, false);
    } else if (profile.speciesCommonName.length() > 0) {
      updateSpeciesQuery(profile.speciesCommonName, false);
    } else {
      updateSpeciesQuery(speciesQuery, false);
    }
    profileStatusText =
        String("Profile restored: ") +
        (profile.speciesCommonName.length() ? profile.speciesCommonName : speciesQuery);
    LOG_INFO(kLogTagMain, "Restored profile for '%s'", speciesQuery.c_str());
  } else {
    updateSpeciesQuery(speciesQuery, false);
    LOG_INFO(kLogTagMain, "No stored profile; using preset '%s'", speciesQuery.c_str());
  }
  wifiStatusText = net::network.statusMessage();

  lastReadings = sensors.sample();
  currentMood = expressionLogic.evaluate(lastReadings);
  scheduleNextBlink(millis());
  LOG_INFO(kLogTagMain, "Initial sensor sample soil=%.1f%% light=%.1f%% temp=%.1fC",
           lastReadings.soilMoisturePct, lastReadings.lightPct, lastReadings.temperatureC);
}

void loop() {
  uint32_t now = millis();

  handleSerialInput();
  net::network.loop();
  web::service.loop();

  // Buttons and menu handling.
  input::ButtonEvent evt = buttons.poll();
  if (evt.type != input::ButtonEventType::None) {
    lastInteractionMs = now;
    if (evt.type == input::ButtonEventType::Click) {
      if (evt.id == input::ButtonId::Both) {
        audioEngine.playChord({523.3f, 659.3f}, 120, 8);
      } else {
        float base = (evt.id == input::ButtonId::Left) ? 622.3f : 783.9f;
        audioEngine.playTone(base, 70);
      }
      ambientResumeAtMs = now + kAmbientResumeDelayMs;
    }
    ui::MenuAction action = menuController.handleEvent(evt);
    LOG_DEBUG(kLogTagMain, "Button event id=%d type=%d", static_cast<int>(evt.id), static_cast<int>(evt.type));
    if (action.openScreen || action.returnToMenu) {
      lastDisplayUpdateMs = 0;
      LOG_INFO(kLogTagMain, "Display mode %s -> screen %d",
               action.returnToMenu ? "menu" : "screen",
               static_cast<int>(action.screen));
    }
    if (action.calibration != ui::CalibrationTarget::None) {
      applyCalibration(action.calibration);
      LOG_INFO(kLogTagMain, "Applied calibration target %d", static_cast<int>(action.calibration));
    }

    if (action.presetDelta != 0) {
      cyclePreset(action.presetDelta);
    }
    if (action.triggerProfileFetch) {
      if (!profileFetchInProgress) {
        profileStatusText = String("Queued fetch: ") + speciesQuery;
      }
      profileFetchRequested = true;
      LOG_INFO(kLogTagMain, "Queued profile fetch (preset delta %d)", action.presetDelta);
    }
    if (action.playDemoChord) {
      audioEngine.playChord({523.3f, 659.3f, 783.9f}, 900, 10);
      LOG_INFO(kLogTagMain, "Demo chord requested");
    }
    if (action.resetProfile) {
      profileManager.clearProfile();
      expressionLogic = brain::ExpressionLogic();
      profileManager.applyTo(expressionLogic);
      currentMood = expressionLogic.evaluate(lastReadings);
      profileStatusText = "Profile cleared. Using defaults.";
      LOG_WARN(kLogTagMain, "Profile reset from menu");
    }
  }

  maybeHandleProfileFetch();

  const ui::MenuState& menuState = menuController.state();

  // Sensor sampling.
  if (now - lastSensorSampleMs >= kSensorIntervalMs) {
    lastReadings = sensors.sample();
    currentMood = expressionLogic.evaluate(lastReadings);
    LOG_DEBUG(kLogTagMain, "Sensor update soil=%.1f%% light=%.1f%% temp=%.1fC hum=%.1f%% mood=%d",
              lastReadings.soilMoisturePct, lastReadings.lightPct, lastReadings.temperatureC,
              lastReadings.humidityPct, static_cast<int>(currentMood.mood));
    if (currentMood.playHydrationCue && !audioEngine.isPlaying()) {
      audioEngine.playChord({392.0f, 523.3f}, 800, 14);
      LOG_INFO(kLogTagMain, "Hydration cue triggered");
    } else if (currentMood.playCelebrationCue && !audioEngine.isPlaying()) {
      audioEngine.playChord({523.3f, 659.3f, 783.9f}, 750, 8);
      LOG_INFO(kLogTagMain, "Celebration cue triggered");
    }
    lastSensorSampleMs = now;
  }

  // Display updates, including blink animation.
  bool blink = updateBlink(now);
  refreshStatusView(now);
  char timeText[6];
  formatClock(timeText, sizeof(timeText));
  web::service.updateState(lastReadings, statusView, speciesQuery, profileFetchInProgress, presetIndex, kPresetCount);
  if (now - lastDisplayUpdateMs >= kDisplayIntervalMs) {
    if (menuState.inMenu || menuState.activeScreen != display::PageId::Mood) {
      timeText[0] = '\0';
    }
    uint32_t interactionAge = (lastInteractionMs == 0) ? 0xFFFFFFFFUL : now - lastInteractionMs;
    if (interactionAge <= 1200UL) {
      currentMood.face.interactionPulseMs = static_cast<uint16_t>(interactionAge);
    } else {
      currentMood.face.interactionPulseMs = 0xFFFF;
    }
    display::MenuListView menuView;
    const display::MenuListView* menuPtr = nullptr;
    if (menuState.inMenu) {
      menuController.buildMenuView(&menuView);
      menuPtr = &menuView;
    }
    display::PageId pageToRender = menuState.inMenu ? display::PageId::Menu : menuState.activeScreen;
    uint8_t screenIndex = menuState.screenIndex;
    uint8_t screenCount = menuState.screenCount;
    displayManager.render(currentMood.face, lastReadings, statusView, menuPtr, timeText, pageToRender, screenIndex,
                          screenCount, blink);
    lastDisplayUpdateMs = now;
  }

  audioEngine.update();

  bool ambientActive = audioEngine.isAmbientActive();
  bool wantAmbient = (!menuState.inMenu && menuState.activeScreen == display::PageId::Mood);

  if (audioEngine.isPlaying() && !ambientActive) {
    ambientResumeAtMs = now + kAmbientResumeDelayMs;
  }

  if (!wantAmbient && ambientActive) {
    audioEngine.stopAmbient();
    ambientActive = false;
    ambientResumeAtMs = now + kAmbientResumeDelayMs;
  }

  if (wantAmbient && !ambientActive && !audioEngine.isPlaying() && now >= ambientResumeAtMs) {
    audioEngine.playAmbientLoop();
    ambientActive = true;
  }

  delay(10);
}


