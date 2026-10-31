// WMO weather interpretation codes as used by Open-Meteo ("weather_code").
// https://open-meteo.com/en/docs (section "WMO Weather interpretation codes")
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

WeatherCategory wmoCategory(int code);

// Short English description (TFT built-in fonts have no Vietnamese glyphs).
const char* wmoDescription(int code);
