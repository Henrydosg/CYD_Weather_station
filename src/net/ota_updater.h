// Firmware updates over WiFi from the project's GitHub Pages
// (config::kOtaManifestUrl, written by tools/release.py).
// Checks once a day (and on request); the user starts the install on the
// Update screen. The download is streamed into the other OTA slot a chunk per
// update(), verified with MD5, then the device restarts into it.
#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdint.h>

#include "config.h"
#include "ota_status.h"

class OtaUpdater {
 public:
  void requestCheck() { checkRequested_ = true; }
  void requestInstall() { installRequested_ = true; }
  // busy: another HTTPS request is due in this loop iteration.
  void update(uint32_t now, bool networkUp, bool busy);
  const OtaStatus& status() const { return status_; }
  bool downloading() const { return status_.state == OtaState::Downloading; }

 private:
  bool check();
  bool beginDownload();
  void continueDownload(uint32_t now);
  void fail(const char* why);
  void setState(OtaState state);

  OtaStatus status_;
  bool checkRequested_ = false;
  bool installRequested_ = false;
  bool wasUp_ = false;
  uint32_t upSince_ = 0;
  bool checked_ = false;
  uint32_t lastCheckAt_ = 0;
  uint32_t restartAt_ = 0;

  // Download
  WiFiClientSecure client_;
  HTTPClient http_;
  char url_[config::kMaxUrlLen + 1] = "";
  char md5_[33] = "";
  uint32_t size_ = 0;
  uint32_t written_ = 0;
  uint32_t lastDataAt_ = 0;
  uint8_t buf_[config::kOtaChunkBytes];
};
