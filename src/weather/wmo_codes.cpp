#include "wmo_codes.h"

WeatherCategory wmoCategory(int code) {
  switch (code) {
    case 0:
      return WeatherCategory::Clear;
    case 1: case 2:
      return WeatherCategory::PartlyCloudy;
    case 3:
      return WeatherCategory::Cloudy;
    case 45: case 48:
      return WeatherCategory::Fog;
    case 51: case 53: case 55: case 56: case 57:
      return WeatherCategory::Drizzle;
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:
      return WeatherCategory::Rain;
    case 71: case 73: case 75: case 77: case 85: case 86:
      return WeatherCategory::Snow;
    case 95: case 96: case 99:
      return WeatherCategory::Thunderstorm;
    default:
      return WeatherCategory::Unknown;
  }
}

const char* wmoDescription(int code) {
  switch (code) {
    case 0:  return "Clear sky";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Dense drizzle";
    case 56: return "Freezing drizzle";
    case 57: return "Dense frz. drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: return "Freezing rain";
    case 67: return "Heavy frz. rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Violent showers";
    case 85: return "Snow showers";
    case 86: return "Heavy snow showers";
    case 95: return "Thunderstorm";
    case 96: return "T-storm, hail";
    case 99: return "T-storm, heavy hail";
    default: return "Unknown";
  }
}
