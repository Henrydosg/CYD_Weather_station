#include "openweathermap.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include "config.h"
#include "owm_codes.h"
#include "owm_root_ca.h"

namespace {

constexpr float kMsToKmh = 3.6f;
constexpr int32_t kSecondsPerDay = 86400;
constexpr int32_t kSecondsPerHour = 3600;
constexpr int32_t kSecondsPerMinute = 60;

String buildUrl(const char* apiKey) {
  String url = config::kOwmBaseUrl;
  url += "?lat=";
  url += config::kLatitude;
  url += "&lon=";
  url += config::kLongitude;
  url += "&units=";
  url += config::kOwmUnits;
  url += "&lang=";
  url += config::kOwmLang;
  url += "&appid=";
  url += apiKey;
  return url;
}

// Local HH:MM from a UTC unix time plus the location's UTC offset.
void formatLocalTime(int64_t utc, int32_t offsetSec, char* out, size_t len) {
  int64_t secOfDay = (utc + offsetSec) % kSecondsPerDay;
  if (secOfDay < 0) {
    secOfDay += kSecondsPerDay;
  }
  snprintf(out, len, "%02d:%02d", static_cast<int>(secOfDay / kSecondsPerHour),
           static_cast<int>((secOfDay % kSecondsPerHour) / kSecondsPerMinute));
}

// strlcpy that never cuts a multi-byte UTF-8 character in half.
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

}  // namespace

void OpenWeatherMapClient::update(uint32_t now, bool networkUp) {
  if (!networkUp) {
    return;
  }

  // Second phase: the UI has had one loop iteration to show "Fetching".
  if (status_ == FetchStatus::Fetching) {
    WeatherData fresh;
    if (fetch(fresh)) {
      data_ = fresh;
      ++version_;
      status_ = FetchStatus::Ok;
      nextDelayMs_ = config::kWeatherRefreshMs;
    } else {
      status_ = FetchStatus::Error;
      nextDelayMs_ = config::kWeatherRetryMs;
    }
    lastAttempt_ = millis();  // the request took time; measure from its end
    return;
  }

  const bool due = (now - lastAttempt_) >= nextDelayMs_;
  if (refreshRequested_ || due) {
    refreshRequested_ = false;
    status_ = FetchStatus::Fetching;
  }
}

bool OpenWeatherMapClient::fetch(WeatherData& out) {
  WiFiClientSecure client;
  client.setCACert(kOwmRootCa);

  HTTPClient http;
  http.setTimeout(config::kHttpTimeoutMs);
  http.setConnectTimeout(config::kHttpTimeoutMs);
  // HTTP/1.0 avoids chunked transfer encoding so the body can be streamed
  // straight into the JSON parser.
  http.useHTTP10(true);

  // The URL contains the API key, so it is never logged.
  Serial.printf("[weather] GET %s (lat %s, lon %s)\n", config::kOwmBaseUrl, config::kLatitude,
                config::kLongitude);
  if (!http.begin(client, buildUrl(apiKey_))) {
    Serial.println("[weather] http.begin failed");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    if (code == HTTP_CODE_UNAUTHORIZED) {
      Serial.println("[weather] HTTP 401: API key invalid or not activated yet");
    } else {
      Serial.printf("[weather] HTTP error %d (%s)\n", code, HTTPClient::errorToString(code).c_str());
    }
    http.end();
    return false;
  }

  // Only keep the fields we use to minimise RAM.
  JsonDocument filter;
  filter["weather"][0]["id"] = true;
  filter["weather"][0]["description"] = true;
  filter["weather"][0]["icon"] = true;
  filter["main"]["temp"] = true;
  filter["main"]["feels_like"] = true;
  filter["main"]["humidity"] = true;
  filter["main"]["pressure"] = true;
  filter["wind"]["speed"] = true;
  filter["dt"] = true;
  filter["timezone"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[weather] JSON error: %s\n", err.c_str());
    return false;
  }

  JsonObjectConst main = doc["main"];
  JsonObjectConst weather = doc["weather"][0];
  if (main.isNull() || !main["temp"].is<float>() || weather.isNull()) {
    Serial.println("[weather] response missing 'main' or 'weather'");
    return false;
  }

  out.temperatureC = main["temp"];
  out.apparentC = main["feels_like"];
  out.humidityPct = main["humidity"];
  out.pressureHpa = main["pressure"];
  out.windKmh = doc["wind"]["speed"].as<float>() * kMsToKmh;

  const int conditionId = weather["id"] | -1;
  out.category = owmCategory(conditionId);
  // Icon codes end in 'd' (day) or 'n' (night), e.g. "04d".
  const char* icon = weather["icon"] | "";
  const size_t iconLen = strlen(icon);
  out.isDay = iconLen == 0 || icon[iconLen - 1] != 'n';

  copyUtf8(out.description, weather["description"] | "", sizeof(out.description));
  // Capitalise an ASCII first letter ("mây đen" -> "Mây đen").
  out.description[0] = static_cast<char>(toupper(static_cast<unsigned char>(out.description[0])));

  formatLocalTime(doc["dt"].as<int64_t>(), doc["timezone"].as<int32_t>(), out.observedAt,
                  sizeof(out.observedAt));

  Serial.printf("[weather] %.1f C, %.0f %%, %.1f km/h, id %d \"%s\" @ %s\n", out.temperatureC,
                out.humidityPct, out.windKmh, conditionId, out.description, out.observedAt);
  return true;
}
