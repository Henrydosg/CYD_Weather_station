#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

const char* wifiStateName(WifiState state) {
  switch (state) {
    case WifiState::Idle:         return "idle";
    case WifiState::Connecting:   return "connecting";
    case WifiState::Connected:    return "connected";
    case WifiState::WaitingRetry: return "waiting retry";
  }
  return "?";
}

WifiManager::WifiManager(const char* ssid, const char* password)
    : ssid_(ssid), password_(password), retryDelayMs_(config::kWifiRetryInitialMs) {}

void WifiManager::begin(uint32_t now) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config::kHostname);
  // Reconnects are driven by this class so the backoff stays predictable.
  WiFi.setAutoReconnect(false);
  startConnect(now);
}

void WifiManager::update(uint32_t now) {
  const bool linkUp = WiFi.status() == WL_CONNECTED;
  const uint32_t elapsed = now - stateSince_;  // rollover-safe

  switch (state_) {
    case WifiState::Idle:
      break;

    case WifiState::Connecting:
      if (linkUp) {
        state_ = WifiState::Connected;
        stateSince_ = now;
        retryDelayMs_ = config::kWifiRetryInitialMs;
        Serial.printf("[wifi] connected, IP %s, RSSI %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
      } else if (elapsed >= config::kWifiConnectTimeoutMs) {
        Serial.println("[wifi] connect timeout");
        scheduleRetry(now);
      }
      break;

    case WifiState::Connected:
      if (!linkUp) {
        Serial.println("[wifi] link lost, reconnecting");
        startConnect(now);
      }
      break;

    case WifiState::WaitingRetry:
      if (elapsed >= retryDelayMs_) {
        retryDelayMs_ = min(retryDelayMs_ * 2, config::kWifiRetryMaxMs);
        startConnect(now);
      }
      break;
  }
}

void WifiManager::startConnect(uint32_t now) {
  Serial.printf("[wifi] connecting to \"%s\"\n", ssid_);
  WiFi.disconnect();
  WiFi.begin(ssid_, password_);
  state_ = WifiState::Connecting;
  stateSince_ = now;
}

void WifiManager::scheduleRetry(uint32_t now) {
  WiFi.disconnect();
  state_ = WifiState::WaitingRetry;
  stateSince_ = now;
  Serial.printf("[wifi] retry in %lu s\n", static_cast<unsigned long>(retryDelayMs_ / 1000));
}
