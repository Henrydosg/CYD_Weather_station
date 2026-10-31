// Open-Meteo client: current weather, 7-day and 24 h forecast plus US AQI for
// every saved city, one request each (all coordinates at once), refreshed
// with millis(). No API key.
#pragma once

#include <stdint.h>

#include "../settings/settings.h"
#include "weather_data.h"

class OpenMeteoClient {
 public:
  // Cities to fetch, in display order. Call again after any change: data of
  // cities that are still in the list is kept, new ones are fetched.
  void setCities(const SavedCities& cities);

  // Call every loop iteration. Each HTTPS request is synchronous but bounded
  // by config::kHttpTimeoutMs; forecast and air quality run in separate loops.
  void update(uint32_t now, bool networkUp);
  void requestRefresh() { refreshRequested_ = true; }

  FetchStatus status() const { return status_; }
  // Incremented whenever any city's data changes.
  uint32_t version() const { return version_; }
  // millis() of the last successful forecast (valid when version() > 0).
  uint32_t lastSuccessMs() const { return lastSuccess_; }
  uint8_t count() const { return count_; }
  const CityWeather* cities() const { return data_; }

 private:
  enum class Step : uint8_t { Forecast, AirQuality };

  bool fetchForecast();
  bool fetchAirQuality();
  // "?latitude=a,b&longitude=c,d" for all cities into buf.
  int appendCoordinates(char* buf, int len, int size) const;

  CityWeather data_[config::kMaxCities];
  uint8_t count_ = 0;
  FetchStatus status_ = FetchStatus::NoData;
  Step step_ = Step::Forecast;
  uint32_t version_ = 0;
  uint32_t lastAttempt_ = 0;
  uint32_t lastSuccess_ = 0;
  uint32_t nextDelayMs_ = 0;
  bool refreshRequested_ = true;  // fetch right after the first connect
};
