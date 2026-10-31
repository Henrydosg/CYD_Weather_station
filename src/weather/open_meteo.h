// Open-Meteo client: periodic, scheduled with millis(), no API key needed.
#pragma once

#include <stdint.h>

struct WeatherData {
  float temperatureC = 0;
  float apparentC = 0;
  float humidityPct = 0;
  float windKmh = 0;
  float todayMaxC = 0;
  float todayMinC = 0;
  int weatherCode = -1;
  bool isDay = true;
  char observedAt[6] = "--:--";  // local HH:MM reported by the API
};

enum class FetchStatus : uint8_t {
  NoData,    // nothing fetched yet
  Fetching,  // request scheduled for this loop iteration
  Ok,
  Error,
};

class OpenMeteoClient {
 public:
  // Call every loop iteration. The HTTP request itself is synchronous but
  // bounded by config::kHttpTimeoutMs.
  void update(uint32_t now, bool networkUp);

  // Ask for a refresh as soon as the network is available.
  void requestRefresh() { refreshRequested_ = true; }

  FetchStatus status() const { return status_; }
  bool hasData() const { return version_ > 0; }
  const WeatherData& data() const { return data_; }
  // Incremented on every successful fetch; lets the UI detect new data.
  uint32_t version() const { return version_; }

 private:
  bool fetch(WeatherData& out);

  WeatherData data_;
  FetchStatus status_ = FetchStatus::NoData;
  uint32_t version_ = 0;
  uint32_t lastAttempt_ = 0;
  uint32_t nextDelayMs_ = 0;
  bool refreshRequested_ = true;  // fetch right after the first connect
};
