// Non-blocking WiFi station manager with automatic reconnect and backoff.
#pragma once

#include <stdint.h>

enum class WifiState : uint8_t {
  Idle,          // begin() not called yet
  Connecting,    // WiFi.begin() issued, waiting for association + IP
  Connected,     // link up, IP assigned
  WaitingRetry,  // connect failed or link lost, waiting for backoff to expire
};

const char* wifiStateName(WifiState state);

class WifiManager {
 public:
  WifiManager(const char* ssid, const char* password);

  void begin(uint32_t now);
  // Call every loop iteration. Never blocks.
  void update(uint32_t now);

  WifiState state() const { return state_; }
  bool isConnected() const { return state_ == WifiState::Connected; }
  // Last sampled signal strength in dBm (0 when not connected).
  int8_t rssi() const { return rssi_; }

 private:
  void startConnect(uint32_t now);
  void scheduleRetry(uint32_t now);

  const char* ssid_;
  const char* password_;
  WifiState state_ = WifiState::Idle;
  uint32_t stateSince_ = 0;
  uint32_t retryDelayMs_;
  int8_t rssi_ = 0;
  uint32_t lastRssiSample_ = 0;
};
