#include "open_meteo.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"

namespace {

String buildUrl() {
  String url = config::kOpenMeteoHost;
  url += "/v1/forecast?latitude=";
  url += config::kLatitude;
  url += "&longitude=";
  url += config::kLongitude;
  url += "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
         "weather_code,wind_speed_10m,is_day";
  url += "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1";
  url += "&timezone=";
  url += config::kTimezone;
  return url;
}

}  // namespace

void OpenMeteoClient::update(uint32_t now, bool networkUp) {
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

bool OpenMeteoClient::fetch(WeatherData& out) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(config::kHttpTimeoutMs);
  http.setConnectTimeout(config::kHttpTimeoutMs);
  // HTTP/1.0 avoids chunked transfer encoding so the body can be streamed
  // straight into the JSON parser.
  http.useHTTP10(true);

  const String url = buildUrl();
  Serial.printf("[weather] GET %s\n", url.c_str());
  if (!http.begin(client, url)) {
    Serial.println("[weather] http.begin failed");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[weather] HTTP error %d (%s)\n", code, HTTPClient::errorToString(code).c_str());
    http.end();
    return false;
  }

  // Only keep the fields we use to minimise RAM.
  JsonDocument filter;
  filter["current"]["time"] = true;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["apparent_temperature"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["current"]["is_day"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[weather] JSON error: %s\n", err.c_str());
    return false;
  }

  JsonObjectConst current = doc["current"];
  if (current.isNull() || !current["temperature_2m"].is<float>()) {
    Serial.println("[weather] response missing 'current'");
    return false;
  }

  out.temperatureC = current["temperature_2m"];
  out.apparentC = current["apparent_temperature"];
  out.humidityPct = current["relative_humidity_2m"];
  out.windKmh = current["wind_speed_10m"];
  out.weatherCode = current["weather_code"] | -1;
  out.isDay = (current["is_day"] | 1) != 0;
  out.todayMaxC = doc["daily"]["temperature_2m_max"][0] | out.temperatureC;
  out.todayMinC = doc["daily"]["temperature_2m_min"][0] | out.temperatureC;

  // "time" looks like "2026-10-31T14:15"; keep the HH:MM part.
  const char* time = current["time"] | "";
  const char* tee = strchr(time, 'T');
  if (tee != nullptr && strlen(tee + 1) >= 5) {
    strlcpy(out.observedAt, tee + 1, sizeof(out.observedAt));
  }

  Serial.printf("[weather] %.1f C, %.0f %%, %.1f km/h, code %d @ %s\n", out.temperatureC,
                out.humidityPct, out.windKmh, out.weatherCode, out.observedAt);
  return true;
}
