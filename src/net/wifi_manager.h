// Non-blocking WiFi station manager: automatic reconnect with backoff over a
// list of candidate networks (saved ones, most recent first, then the
// default from secrets.h), user-initiated connects and asynchronous scans.
#pragma once

#include <stdint.h>

#include "../settings/settings.h"
#include "config.h"

enum class WifiState : uint8_t {
  Idle,          // begin() not called yet
  Connecting,    // WiFi.begin() issued, waiting for association + IP
  Connected,     // link up, IP assigned
  WaitingRetry,  // every candidate failed, waiting for backoff to expire
};

enum class ScanState : uint8_t { Idle, Scanning, Done, Failed };

// Outcome of the last connectTo() request (user picked a network).
enum class ConnectAttempt : uint8_t { None, Pending, Succeeded, Failed };

struct WifiNetworkInfo {
  char ssid[config::kMaxSsidLen + 1];
  int8_t rssi;   // dBm
  bool secured;  // false = open network
};

struct WifiScanResults {
  uint8_t count = 0;
  WifiNetworkInfo items[config::kMaxScanResults];  // strongest first, unique SSIDs
};

const char* wifiStateName(WifiState state);

class WifiManager {
 public:
  WifiManager(const char* defaultSsid, const char* defaultPassword);

  // Saved networks are tried before the default one. Must outlive this object.
  void setSavedNetworks(const SavedNetworks* saved) { saved_ = saved; }

  void begin(uint32_t now);
  // Call every loop iteration. Never blocks.
  void update(uint32_t now);

  // Connects to a network chosen by the user. On failure the manager falls
  // back to the candidate list; attempt() reports the result.
  void connectTo(const char* ssid, const char* password, uint32_t now);
  // Starts an asynchronous scan (deferred while a connect is in progress).
  void startScan();

  WifiState state() const { return state_; }
  bool isConnected() const { return state_ == WifiState::Connected; }
  // Last sampled signal strength in dBm (0 when not connected).
  int8_t rssi() const { return rssi_; }
  // Network currently connected or being connected to.
  const char* ssid() const { return ssid_; }
  // Dotted IP address, "" when not connected.
  const char* ip() const { return ip_; }

  ScanState scanState() const { return scanState_; }
  // A scan runs or is about to start: the radio is busy, so no HTTPS meanwhile
  // (a request during a scan fails, and the scan overruns its time limit).
  bool scanBusy() const { return scanRequested_ || scanState_ == ScanState::Scanning; }
  const WifiScanResults& scanResults() const { return scan_; }
  // Incremented whenever the scan state or results change.
  uint32_t scanVersion() const { return scanVersion_; }
  ConnectAttempt attempt() const { return attempt_; }

 private:
  uint8_t candidateCount() const;
  void selectCandidate(uint8_t index);
  void startConnect(uint32_t now);
  void connectFailed(uint32_t now);
  void scheduleRetry(uint32_t now);
  void pollScan(uint32_t now);
  void collectScan(int found);

  const char* defaultSsid_;
  const char* defaultPassword_;
  const SavedNetworks* saved_ = nullptr;

  // Current target (copied: candidates and UI buffers may change).
  char ssid_[config::kMaxSsidLen + 1] = "";
  char password_[config::kMaxPasswordLen + 1] = "";
  uint8_t candidate_ = 0;
  bool manual_ = false;  // current attempt comes from connectTo()

  WifiState state_ = WifiState::Idle;
  uint32_t stateSince_ = 0;
  uint32_t retryDelayMs_;
  int8_t rssi_ = 0;
  char ip_[16] = "";
  uint32_t lastRssiSample_ = 0;
  ConnectAttempt attempt_ = ConnectAttempt::None;

  ScanState scanState_ = ScanState::Idle;
  bool scanRequested_ = false;
  uint32_t scanStarted_ = 0;
  uint8_t emptyScans_ = 0;  // consecutive empty results of the current scan request
  uint32_t scanVersion_ = 0;
  WifiScanResults scan_;
};
