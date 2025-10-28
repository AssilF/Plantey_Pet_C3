#include "network_manager.h"

#include <WiFiClientSecure.h>
#include <cstring>

#include "logging.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets_template.h"
#endif

namespace net {
namespace {
constexpr unsigned long kRetryIntervalMs = 10000;
constexpr unsigned long kConnectionTimeoutMs = 20000;
const IPAddress kApIp(192, 168, 4, 1);
const IPAddress kApGateway(192, 168, 4, 1);
const IPAddress kApSubnet(255, 255, 255, 0);

const char* defaultApSsid() {
  return "PlanteyPet";
}

const char* defaultApPassword() {
  return "planteypet";
}
}  // namespace

constexpr const char* kLogTagNet = "net";

void NetworkManager::begin() {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.setAutoReconnect(true);
  WiFi.setAutoConnect(true);

  const char* apSsid = (secrets::AP_SSID != nullptr && secrets::AP_SSID[0] != '\0') ? secrets::AP_SSID : defaultApSsid();
  const char* apPassword =
      (secrets::AP_PASSWORD != nullptr && secrets::AP_PASSWORD[0] != '\0') ? secrets::AP_PASSWORD : defaultApPassword();

  WiFi.softAPConfig(kApIp, kApGateway, kApSubnet);
  bool apOk = false;
  if (std::strlen(apPassword) >= 8) {
    apOk = WiFi.softAP(apSsid, apPassword);
  } else {
    apOk = WiFi.softAP(apSsid);
  }
  if (apOk) {
    WiFi.softAPsetHostname(apSsid);
    apStarted_ = true;
    statusMessage_ = String("AP: ") + apSsid;
    LOG_INFO(kLogTagNet, "SoftAP started SSID='%s' IP=%s", apSsid, WiFi.softAPIP().toString().c_str());
  } else {
    statusMessage_ = "AP start failed";
    LOG_ERROR(kLogTagNet, "Failed to start SoftAP");
  }
  attemptingConnection_ = false;
  lastAttemptMs_ = millis();
}

void NetworkManager::loop() {
  ensureConnected();
}

bool NetworkManager::ensureConnected() {
  if (!credentialsConfigured()) {
    statusMessage_ = apStarted_ ? "AP only (no STA creds)" : "WiFi AP inactive";
    attemptingConnection_ = false;
    static bool warned = false;
    if (!warned) {
      LOG_WARN(kLogTagNet, "Station credentials missing; operating in AP-only mode");
      warned = true;
    }
    return false;
  }

  if (WiFi.isConnected()) {
    IPAddress ip = WiFi.localIP();
    statusMessage_ = String("STA ") + ip.toString();
    attemptingConnection_ = false;
    static IPAddress lastIp;
    if (lastIp != ip) {
      LOG_INFO(kLogTagNet, "Station connected, IP=%s", ip.toString().c_str());
      lastIp = ip;
    }
    return true;
  }

  unsigned long now = millis();
  if (!attemptingConnection_ || (now - lastAttemptMs_) > kRetryIntervalMs) {
    WiFi.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
    statusMessage_ = "STA connecting...";
    attemptingConnection_ = true;
    lastAttemptMs_ = now;
    LOG_INFO(kLogTagNet, "Attempting STA connection to '%s'", secrets::WIFI_SSID);
  } else if ((now - lastAttemptMs_) > kConnectionTimeoutMs) {
    statusMessage_ = "STA retry soon";
    attemptingConnection_ = false;
    LOG_WARN(kLogTagNet, "STA connection timeout, will retry");
  }
  return false;
}

bool NetworkManager::credentialsConfigured() const {
  return secrets::WIFI_SSID != nullptr && secrets::WIFI_PASSWORD != nullptr && secrets::WIFI_SSID[0] != '\0' &&
         secrets::WIFI_PASSWORD[0] != '\0' && std::strcmp(secrets::WIFI_SSID, "YourWifiSsid") != 0 &&
         std::strcmp(secrets::WIFI_PASSWORD, "YourWifiPassword") != 0;
}

NetworkManager network;

}  // namespace net
