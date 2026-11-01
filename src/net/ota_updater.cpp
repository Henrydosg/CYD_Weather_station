#include "ota_updater.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <stdio.h>
#include <string.h>

#include "../weather/https_json.h"
#include "../weather/open_meteo_root_ca.h"

namespace {

constexpr int kVersionParts = 3;
constexpr size_t kMd5Len = 32;
constexpr uint8_t kPercent = 100;

// "1.2.3" -> comparable number; false when malformed.
bool parseVersion(const char* text, uint32_t& out) {
  unsigned major = 0, minor = 0, patch = 0;
  if (sscanf(text, "%u.%u.%u", &major, &minor, &patch) != kVersionParts || major > 255 ||
      minor > 255 || patch > 255) {
    return false;
  }
  out = (major << 16) | (minor << 8) | patch;
  return true;
}

}  // namespace

void OtaUpdater::setState(OtaState state) {
  status_.state = state;
  ++status_.version;
}

void OtaUpdater::fail(const char* why) {
  Serial.printf("[ota] failed: %s\n", why);
  if (Update.isRunning()) {
    Update.abort();
  }
  http_.end();
  setState(OtaState::Failed);
}

void OtaUpdater::update(uint32_t now, bool networkUp, bool busy) {
  switch (status_.state) {
    case OtaState::Downloading:
      continueDownload(now);
      return;
    case OtaState::Restarting:
      if (now - restartAt_ >= config::kOtaRebootDelayMs) {
        ESP.restart();
      }
      return;
    default:
      break;
  }

  if (networkUp && !wasUp_) {
    upSince_ = now;
  }
  wasUp_ = networkUp;
  if (!networkUp) {
    if (installRequested_ || checkRequested_) {
      installRequested_ = checkRequested_ = false;
      fail("no WiFi");
    }
    return;
  }
  if (busy) {
    return;
  }

  if (installRequested_) {
    installRequested_ = false;
    if (status_.state == OtaState::Available) {
      beginDownload();  // on failure fail() has set OtaState::Failed
    }
    return;
  }

  const bool firstDue = !checked_ && now - upSince_ >= config::kOtaFirstCheckMs;
  const bool dailyDue = checked_ && now - lastCheckAt_ >= config::kOtaCheckIntervalMs;
  if (status_.state == OtaState::Checking || checkRequested_ || firstDue || dailyDue) {
    // A requested check shows "Checking" for one loop iteration first.
    if (checkRequested_ && status_.state != OtaState::Checking) {
      checkRequested_ = false;
      setState(OtaState::Checking);
      return;
    }
    checkRequested_ = false;
    checked_ = true;
    lastCheckAt_ = now;
    if (!check()) {
      fail("manifest");
    }
  }
}

bool OtaUpdater::check() {
  JsonDocument filter;
  for (const char* key : {"version", "url", "size", "md5"}) {
    filter[key] = true;
  }
  JsonDocument doc;
  if (!httpsGetJson(config::kOtaManifestUrl, filter, doc, "ota")) {
    return false;
  }
  const char* latest = doc["version"] | "";
  const char* url = doc["url"] | "";
  const char* md5 = doc["md5"] | "";
  const uint32_t size = doc["size"] | 0;
  uint32_t current = 0, offered = 0;
  if (!parseVersion(config::kFirmwareVersion, current) || !parseVersion(latest, offered) ||
      strncmp(url, "https://", strlen("https://")) != 0 || strlen(url) > config::kMaxUrlLen ||
      strlen(md5) != kMd5Len || size == 0) {
    Serial.println("[ota] manifest incomplete");
    return false;
  }
  strlcpy(status_.latest, latest, sizeof(status_.latest));
  strlcpy(url_, url, sizeof(url_));
  strlcpy(md5_, md5, sizeof(md5_));
  size_ = size;
  Serial.printf("[ota] running %s, published %s\n", config::kFirmwareVersion, latest);
  setState(offered > current ? OtaState::Available : OtaState::UpToDate);
  return true;
}

bool OtaUpdater::beginDownload() {
  Serial.printf("[ota] downloading %s (%lu bytes)\n", url_, static_cast<unsigned long>(size_));
  client_.setCACert(kOpenMeteoRootCa);
  http_.setTimeout(config::kHttpTimeoutMs);
  http_.setConnectTimeout(config::kHttpTimeoutMs);
  http_.useHTTP10(true);  // plain body, no chunked encoding
  if (!http_.begin(client_, url_)) {
    fail("http.begin");
    return false;
  }
  const int code = http_.GET();  // bounded by kHttpTimeoutMs, like the weather requests
  if (code != HTTP_CODE_OK) {
    Serial.printf("[ota] HTTP %d\n", code);
    fail("download");
    return false;
  }
  if (http_.getSize() != static_cast<int>(size_)) {
    fail("size mismatch");
    return false;
  }
  if (!Update.begin(size_)) {
    fail(Update.errorString());
    return false;
  }
  Update.setMD5(md5_);
  written_ = 0;
  lastDataAt_ = millis();
  status_.progressPct = 0;
  setState(OtaState::Downloading);
  return true;
}

void OtaUpdater::continueDownload(uint32_t now) {
  WiFiClient* stream = http_.getStreamPtr();
  const int avail = stream != nullptr ? stream->available() : 0;
  if (avail > 0) {
    const size_t want = min(static_cast<size_t>(avail), sizeof(buf_));
    const size_t n = stream->readBytes(buf_, min(want, static_cast<size_t>(size_ - written_)));
    if (Update.write(buf_, n) != n) {
      fail(Update.errorString());
      return;
    }
    written_ += n;
    lastDataAt_ = now;
    const uint8_t pct = static_cast<uint8_t>(static_cast<uint64_t>(written_) * kPercent / size_);
    if (pct != status_.progressPct) {
      status_.progressPct = pct;
      ++status_.version;
    }
  } else if (now - lastDataAt_ >= config::kOtaStallTimeoutMs) {
    fail("stalled");
    return;
  }

  if (written_ < size_) {
    return;
  }
  http_.end();
  if (!Update.end()) {  // checks the MD5, then makes the new slot bootable
    fail(Update.errorString());
    return;
  }
  Serial.printf("[ota] installed %s, restarting\n", status_.latest);
  restartAt_ = now;
  setState(OtaState::Restarting);
}
