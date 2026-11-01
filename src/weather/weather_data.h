// Provider-independent weather model shared by the weather client and the UI.
#pragma once

#include <stdint.h>

#include "config.h"

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

// WMO weather interpretation code (0..99) -> icon category.
WeatherCategory wmoCategory(uint8_t code);

struct DailyForecast {
  uint8_t mday;     // local date
  uint8_t month;    // 1..12
  uint8_t wday;     // 0 = Sunday
  uint8_t wmoCode;
  float maxC;
  float minC;
};

struct HourlyPoint {
  uint8_t hour;  // local hour 0..23
  uint8_t wmoCode;
  bool isDay;
  uint8_t precipPct;  // probability of precipitation
  float tempC;
};

// Everything shown for one city.
struct CityWeather {
  bool valid = false;
  float latitude = 0;  // the request coordinates (to match saved cities)
  float longitude = 0;
  int32_t utcOffsetSec = 0;  // city local time = UTC + this

  float temperatureC = 0;
  float apparentC = 0;
  float humidityPct = 0;
  float windKmh = 0;
  uint16_t windDirDeg = 0;  // meteorological: direction the wind comes from
  float pressureHpa = 0;
  uint8_t cloudPct = 0;
  float uvIndex = 0;
  int16_t usAqi = -1;  // -1 = unknown
  uint8_t wmoCode = 0;
  bool isDay = true;
  int16_t sunriseMin = -1;  // local minutes after midnight, -1 = unknown
  int16_t sunsetMin = -1;
  float precipitationMm = 0;  // current (preceding 15 minutes)

  // Precipitation in 15-minute steps starting at nowcastStartUtc, in 0.1 mm.
  uint32_t nowcastStartUtc = 0;
  uint8_t nowcastCount = 0;
  uint8_t nowcastTenthsMm[config::kNowcastSlots] = {};

  uint8_t dailyCount = 0;
  DailyForecast daily[config::kForecastDays];
  uint8_t hourlyCount = 0;
  HourlyPoint hourly[config::kForecastHours / config::kHourlyStep];
};

// Rain in the next hours, from the nowcast.
enum class RainKind : uint8_t {
  None,       // dry for the whole nowcast, or no data
  Soon,       // rain expected at atUtc
  Raining,    // raining now, expected to stop at atUtc
  Continues,  // raining now and for the whole nowcast
};
struct RainOutlook {
  RainKind kind;
  uint32_t atUtc;
};
RainOutlook rainOutlook(const CityWeather& w, uint32_t utcNow);

enum class FetchStatus : uint8_t {
  NoData,    // nothing fetched yet
  Fetching,  // request scheduled for this loop iteration
  Ok,
  Error,
};

// City search (geocoding)
struct GeoResult {
  uint32_t id;  // GeoNames id
  char name[config::kMaxCityNameLen + 1];
  char region[config::kMaxCityRegionLen + 1];  // "admin1, country"
  float latitude;
  float longitude;
  uint32_t population;  // 0 = unknown
};

enum class SearchStatus : uint8_t { Idle, Searching, Done, Failed };
