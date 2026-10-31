// OpenWeatherMap client (Current Weather API 2.5) over HTTPS,
// scheduled with millis().
#pragma once

#include <stdint.h>

#include "weather_data.h"

class OpenWeatherMapClient {
 public:
  explicit OpenWeatherMapClient(const char* apiKey) : apiKey_(apiKey) {}

  // Call every loop iteration. The HTTPS request itself is synchronous but
  // bounded by config::kHttpTimeoutMs.
  void update(uint32_t now, bool networkUp);

  // Ask for a refresh as soon as the network is available.
  void requestRefresh() { refreshRequested_ = true; }

  // OpenWeatherMap "lang" code for descriptions ("vi", "en"); refetches.
  void setLanguage(const char* code) {
    lang_ = code;
    requestRefresh();
  }

  FetchStatus status() const { return status_; }
  bool hasData() const { return version_ > 0; }
  const WeatherData& data() const { return data_; }
  // Incremented on every successful fetch; lets the UI detect new data.
  uint32_t version() const { return version_; }
  // millis() of the last successful fetch (valid when hasData()).
  uint32_t lastSuccessMs() const { return lastSuccess_; }

 private:
  bool fetch(WeatherData& out);

  const char* apiKey_;
  const char* lang_ = "vi";
  WeatherData data_;
  FetchStatus status_ = FetchStatus::NoData;
  uint32_t version_ = 0;
  uint32_t lastAttempt_ = 0;
  uint32_t lastSuccess_ = 0;
  uint32_t nextDelayMs_ = 0;
  bool refreshRequested_ = true;  // fetch right after the first connect
};
