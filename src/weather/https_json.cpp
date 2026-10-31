#include "https_json.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "open_meteo_root_ca.h"

bool httpsGetJson(const char* url, const JsonDocument& filter, JsonDocument& doc,
                  const char* tag) {
  WiFiClientSecure client;
  client.setCACert(kOpenMeteoRootCa);

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
  if (code != HTTP_CODE_OK) {
    Serial.printf("[%s] HTTP error %d (%s)\n", tag, code, HTTPClient::errorToString(code).c_str());
    http.end();
    return false;
  }
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
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
