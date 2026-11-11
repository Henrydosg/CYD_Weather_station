#include "https_json.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "open_meteo_root_ca.h"

namespace {

constexpr size_t kReadChunk = 1024;  // bytes per TLS read
// Static, not on the task stack (TLS needs it); only the network task makes requests.
uint8_t gChunk[kReadChunk];
constexpr uint32_t kPollMs = 1;      // wait between checks while no data has arrived

// ArduinoJson reads a Stream one character at a time; on WiFiClientSecure each
// of those is a separate TLS read, which made a forecast take seconds. This
// reader (ArduinoJson's custom reader interface: read() / readBytes()) pulls
// what has arrived in chunks of up to kReadChunk bytes instead. A refill waits
// at most kHttpTimeoutMs for data and stops when the server has closed; the
// whole body stops after kHttpBodyMaxMs. Waiting feeds the task watchdog (a
// no-op for a task that is not watched).
class ChunkReader {
 public:
  explicit ChunkReader(WiFiClient& client) : client_(client), begin_(millis()) {}

  int read() {
    if (pos_ >= len_ && !fill()) {
      return -1;
    }
    return buf_[pos_++];
  }

  size_t readBytes(char* out, size_t length) {
    size_t n = 0;
    while (n < length) {
      if (pos_ >= len_ && !fill()) {
        break;
      }
      size_t take = len_ - pos_;
      if (take > length - n) take = length - n;
      memcpy(out + n, buf_ + pos_, take);
      pos_ += take;
      n += take;
    }
    return n;
  }

  uint32_t bytes() const { return total_; }

 private:
  bool fill() {
    const uint32_t start = millis();
    for (;;) {
      if (millis() - begin_ >= config::kHttpBodyMaxMs) {
        return false;
      }
      const int avail = client_.available();
      if (avail > 0) {
        const size_t want = static_cast<size_t>(avail) < kReadChunk ? avail : kReadChunk;
        const int got = client_.read(buf_, want);
        if (got > 0) {
          pos_ = 0;
          len_ = static_cast<size_t>(got);
          total_ += static_cast<uint32_t>(got);
          return true;
        }
      } else if (!client_.connected()) {
        return false;
      }
      if (millis() - start >= config::kHttpTimeoutMs) {
        return false;
      }
      esp_task_wdt_reset();
      delay(kPollMs);
    }
  }

  WiFiClient& client_;
  const uint32_t begin_;
  uint8_t* const buf_ = gChunk;
  size_t pos_ = 0, len_ = 0;
  uint32_t total_ = 0;
};

}  // namespace

bool httpsGetJson(const char* url, const JsonDocument& filter, JsonDocument& doc,
                  const char* tag) {
  const uint32_t t0 = millis();
  WiFiClientSecure client;
  client.setCACert(kOpenMeteoRootCa);
  client.setHandshakeTimeout(config::kTlsHandshakeSec);

  HTTPClient http;
  http.setTimeout(config::kHttpTimeoutMs);
  http.setConnectTimeout(config::kHttpTimeoutMs);
  // HTTP/1.0 avoids chunked transfer encoding so the body can be streamed
  // straight into the JSON parser.
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    Serial.printf("[%s] http.begin failed\n", tag);
    return false;
  }
  const int code = http.GET();
  const uint32_t t1 = millis();
  esp_task_wdt_reset();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[%s] HTTP error %d (%s) after %lu ms, heap %u (largest %u)\n", tag, code,
                  HTTPClient::errorToString(code).c_str(), static_cast<unsigned long>(t1 - t0),
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    http.end();
    return false;
  }
  ChunkReader reader(http.getStream());
  const DeserializationError err =
      deserializeJson(doc, reader, DeserializationOption::Filter(filter));
  const uint32_t t2 = millis();
  http.end();
  Serial.printf("[%s] %lu ms: connect+GET %lu, body %lu (%lu bytes); heap min %u, largest %u\n",
                tag, static_cast<unsigned long>(t2 - t0), static_cast<unsigned long>(t1 - t0),
                static_cast<unsigned long>(t2 - t1), static_cast<unsigned long>(reader.bytes()),
                ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
  if (err) {
    Serial.printf("[%s] JSON error: %s\n", tag, err.c_str());
    return false;
  }
  return true;
}

void copyUtf8(char* dst, const char* src, size_t size) {
  if (strlcpy(dst, src, size) < size) {
    return;
  }
  size_t len = strlen(dst);
  // Drop trailing continuation bytes and the lead byte of the cut character.
  while (len > 0 && (static_cast<unsigned char>(dst[len - 1]) & 0xC0) == 0x80) {
    --len;
  }
  if (len > 0 && (static_cast<unsigned char>(dst[len - 1]) & 0x80) != 0) {
    --len;
  }
  dst[len] = '\0';
}
