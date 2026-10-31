// Provider-independent weather model shared by the weather client and the UI.
#pragma once

#include <stdint.h>

enum class WeatherCategory : uint8_t {
  Clear,
  PartlyCloudy,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Snow,
  Thunderstorm,
  Unknown,
};

struct WeatherData {
  float temperatureC = 0;
  float apparentC = 0;
  float humidityPct = 0;
  float windKmh = 0;
  float pressureHpa = 0;
  WeatherCategory category = WeatherCategory::Unknown;
  bool isDay = true;
  char description[32] = "";
  char observedAt[6] = "--:--";  // local HH:MM of the observation
};

enum class FetchStatus : uint8_t {
  NoData,    // nothing fetched yet
  Fetching,  // request scheduled for this loop iteration
  Ok,
  Error,
};
