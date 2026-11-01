#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

const char* wifiStateName(WifiState state) {
  switch (state) {
    case WifiState::Idle:         return "idle";
    case WifiState::Connecting:   return "connecting";
    case WifiState::Connected:    return "connected";
    case WifiState::WaitingRetry: return "waiting retry";
  }
  return "?";
}

WifiManager::WifiManager(const char* defaultSsid, const char* defaultPassword)
    : defaultSsid_(defaultSsid),
      defaultPassword_(defaultPassword),
      retryDelayMs_(config::kWifiRetryInitialMs) {}

void WifiManager::begin(uint32_t now) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(config::kHostname);
  // Modem sleep wakes the radio ~10 times a second; the current pulses make
  // the backlight (fed from the same 3.3 V rail) shimmer. Always powered here.
  WiFi.setSleep(false);
  // Reconnects are driven by this class so the backoff stays predictable.
  WiFi.setAutoReconnect(false);
  if (candidateCount() == 0) {
    Serial.println("[wifi] no network yet: choose one in Settings -> WiFi");
    return;  // Idle until connectTo()
  }
  selectCandidate(0);
  startConnect(now);
}

// Candidates: saved networks (most recent first), then the default network
// unless it is already one of the saved ones.
uint8_t WifiManager::candidateCount() const {
  const uint8_t saved = saved_ != nullptr ? saved_->count : 0;
  const bool hasDefault = defaultSsid_[0] != '\0';  // "" in release builds
  const bool defaultSaved = saved_ != nullptr && findNetwork(*saved_, defaultSsid_) != nullptr;
  return saved + (hasDefault && !defaultSaved ? 1 : 0);
}

void WifiManager::selectCandidate(uint8_t index) {
  candidate_ = index;
  manual_ = false;
  const uint8_t saved = saved_ != nullptr ? saved_->count : 0;
  if (index < saved) {
    strlcpy(ssid_, saved_->items[index].ssid, sizeof(ssid_));
    strlcpy(password_, saved_->items[index].password, sizeof(password_));
  } else {
    strlcpy(ssid_, defaultSsid_, sizeof(ssid_));
    strlcpy(password_, defaultPassword_, sizeof(password_));
  }
}

void WifiManager::connectTo(const char* ssid, const char* password, uint32_t now) {
  if (scanState_ == ScanState::Scanning) {
    WiFi.scanDelete();  // a connect would fail while the radio is scanning
    scanState_ = ScanState::Idle;
    ++scanVersion_;
  }
  strlcpy(ssid_, ssid, sizeof(ssid_));
  strlcpy(password_, password, sizeof(password_));
  manual_ = true;
  attempt_ = ConnectAttempt::Pending;
  rssi_ = 0;
  ip_[0] = '\0';
  startConnect(now);
}

void WifiManager::startScan() { scanRequested_ = true; }

void WifiManager::update(uint32_t now) {
  pollScan(now);

  const wl_status_t status = WiFi.status();
  const bool linkUp = status == WL_CONNECTED;
  const uint32_t elapsed = now - stateSince_;  // rollover-safe

  switch (state_) {
    case WifiState::Idle:
      break;

    case WifiState::Connecting: {
      const bool rejected = (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) &&
                            elapsed >= config::kWifiFailGraceMs;
      if (linkUp) {
        state_ = WifiState::Connected;
        stateSince_ = now;
        retryDelayMs_ = config::kWifiRetryInitialMs;
        if (manual_) {
          attempt_ = ConnectAttempt::Succeeded;
          manual_ = false;
        }
        strlcpy(ip_, WiFi.localIP().toString().c_str(), sizeof(ip_));
        Serial.printf("[wifi] connected to \"%s\", IP %s, RSSI %d dBm\n", ssid_, ip_,
                      WiFi.RSSI());
      } else if (rejected || elapsed >= config::kWifiConnectTimeoutMs) {
        Serial.printf("[wifi] \"%s\": %s\n", ssid_, rejected ? "rejected" : "timeout");
        connectFailed(now);
      }
      break;
    }

    case WifiState::Connected:
      if (!linkUp) {
        Serial.println("[wifi] link lost, reconnecting");
        rssi_ = 0;
        ip_[0] = '\0';
        startConnect(now);
      } else if (rssi_ == 0 || now - lastRssiSample_ >= config::kRssiSampleMs) {
        rssi_ = static_cast<int8_t>(WiFi.RSSI());
        lastRssiSample_ = now;
      }
      break;

    case WifiState::WaitingRetry:
      // A running scan owns the radio; retry once it has finished.
      if (scanState_ != ScanState::Scanning && elapsed >= retryDelayMs_) {
        retryDelayMs_ = min(retryDelayMs_ * 2, config::kWifiRetryMaxMs);
        selectCandidate(0);
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

void WifiManager::connectFailed(uint32_t now) {
  if (manual_) {
    // The user's choice failed: report it, then go back to the known networks.
    attempt_ = ConnectAttempt::Failed;
    if (candidateCount() == 0) {
      WiFi.disconnect();
      state_ = WifiState::Idle;
      return;
    }
    selectCandidate(0);
    startConnect(now);
  } else if (candidate_ + 1 < candidateCount()) {
    selectCandidate(candidate_ + 1);
    startConnect(now);
  } else {
    selectCandidate(0);
    scheduleRetry(now);
  }
}

void WifiManager::scheduleRetry(uint32_t now) {
  WiFi.disconnect();
  state_ = WifiState::WaitingRetry;
  stateSince_ = now;
  Serial.printf("[wifi] retry in %lu s\n", static_cast<unsigned long>(retryDelayMs_ / 1000));
}

void WifiManager::pollScan(uint32_t now) {
  if (scanState_ == ScanState::Scanning) {
    const int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) {
      if (now - scanStarted_ >= config::kWifiScanTimeoutMs) {
        Serial.println("[wifi] scan timeout");
        WiFi.scanDelete();
        scanState_ = ScanState::Failed;
        ++scanVersion_;
      }
      return;
    }
    if (found < 0) {
      Serial.println("[wifi] scan failed");
      scanState_ = ScanState::Failed;
    } else {
      collectScan(found);
      scanState_ = ScanState::Done;
    }
    WiFi.scanDelete();
    ++scanVersion_;
    return;
  }

  // Start a requested scan unless a connect attempt is using the radio.
  if (scanRequested_ && state_ != WifiState::Connecting) {
    scanRequested_ = false;
    const int16_t r = WiFi.scanNetworks(true /*async*/);
    scanState_ = r == WIFI_SCAN_FAILED ? ScanState::Failed : ScanState::Scanning;
    scanStarted_ = now;
    ++scanVersion_;
    Serial.println(scanState_ == ScanState::Failed ? "[wifi] scan could not start"
                                                   : "[wifi] scanning");
  }
}

void WifiManager::collectScan(int found) {
  scan_.count = 0;
  for (int i = 0; i < found; ++i) {
    WifiNetworkInfo info{};
    strlcpy(info.ssid, WiFi.SSID(i).c_str(), sizeof(info.ssid));
    if (info.ssid[0] == '\0') {
      continue;  // hidden network
    }
    info.rssi = static_cast<int8_t>(WiFi.RSSI(i));
    info.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

    // Same SSID on several access points / bands: keep the strongest.
    int existing = -1;
    for (int j = 0; j < scan_.count; ++j) {
      if (strcmp(scan_.items[j].ssid, info.ssid) == 0) {
        existing = j;
        break;
      }
    }
    if (existing >= 0) {
      if (scan_.items[existing].rssi >= info.rssi) {
        continue;
      }
      for (int j = existing; j + 1 < scan_.count; ++j) {
        scan_.items[j] = scan_.items[j + 1];
      }
      --scan_.count;
    }

    // Insert sorted by signal strength; the weakest drops off when full.
    int pos = scan_.count;
    while (pos > 0 && scan_.items[pos - 1].rssi < info.rssi) {
      --pos;
    }
    if (pos >= config::kMaxScanResults) {
      continue;
    }
    const int last = scan_.count < config::kMaxScanResults ? scan_.count : scan_.count - 1;
    for (int j = last; j > pos; --j) {
      scan_.items[j] = scan_.items[j - 1];
    }
    scan_.items[pos] = info;
    if (scan_.count < config::kMaxScanResults) {
      ++scan_.count;
    }
  }
  Serial.printf("[wifi] scan: %d found, %u listed\n", found, scan_.count);
}
