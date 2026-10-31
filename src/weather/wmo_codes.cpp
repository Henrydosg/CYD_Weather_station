#include "weather_data.h"

WeatherCategory wmoCategory(uint8_t code) {
  // https://open-meteo.com/en/docs (WMO Weather interpretation codes)
  switch (code) {
    case 0:  return WeatherCategory::Clear;
    case 1:
    case 2:  return WeatherCategory::PartlyCloudy;
    case 3:  return WeatherCategory::Cloudy;
    case 45:
    case 48: return WeatherCategory::Fog;
    case 51: case 53: case 55:
    case 56: case 57: return WeatherCategory::Drizzle;
    case 61: case 63: case 65:
    case 66: case 67:
    case 80: case 81: case 82: return WeatherCategory::Rain;
    case 71: case 73: case 75: case 77:
    case 85: case 86: return WeatherCategory::Snow;
    case 95: case 96: case 99: return WeatherCategory::Thunderstorm;
    default: return WeatherCategory::Unknown;
  }
}
