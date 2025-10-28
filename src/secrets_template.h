#pragma once

// Rename this file to secrets.h and fill in your credentials.
namespace secrets {

constexpr const char* WIFI_SSID = "YourWifiSsid";
constexpr const char* WIFI_PASSWORD = "YourWifiPassword";

// Access point credentials used when the ESP32 hosts its own hotspot.
constexpr const char* AP_SSID = "PlanteyPet";
constexpr const char* AP_PASSWORD = "planteypet";

// Provision an API key from OpenAI (https://platform.openai.com/).
// Keep the key private; do not commit secrets.h to version control.
constexpr const char* OPENAI_API_KEY = "sk-your-key";

// Default model used for plant configuration. Adjust if necessary.
constexpr const char* OPENAI_MODEL = "gpt-4o-mini";

}  // namespace secrets
