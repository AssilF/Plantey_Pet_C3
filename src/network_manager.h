#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace net {

class NetworkManager {
 public:
  void begin();
  void loop();
  bool ensureConnected();
  bool isConnected() const { return WiFi.isConnected(); }
  String statusMessage() const { return statusMessage_; }
  bool apActive() const { return apStarted_; }
  IPAddress apIp() const { return WiFi.softAPIP(); }

 private:
  bool credentialsConfigured() const;

  String statusMessage_ = "WiFi idle";
  unsigned long lastAttemptMs_ = 0;
  bool attemptingConnection_ = false;
  bool apStarted_ = false;
  bool apAnnounced_ = false;
};

extern NetworkManager network;

}  // namespace net
