#include "owm_codes.h"

WeatherCategory owmCategory(int id) {
  // Groups: 2xx thunderstorm, 3xx drizzle, 5xx rain, 6xx snow,
  // 7xx atmosphere (mist, fog, haze...), 800 clear, 80x clouds.
  if (id >= 200 && id < 300) return WeatherCategory::Thunderstorm;
  if (id >= 300 && id < 400) return WeatherCategory::Drizzle;
  if (id == 511) return WeatherCategory::Snow;  // freezing rain
  if (id >= 500 && id < 600) return WeatherCategory::Rain;
  if (id >= 600 && id < 700) return WeatherCategory::Snow;
  if (id >= 700 && id < 800) return WeatherCategory::Fog;
  if (id == 800) return WeatherCategory::Clear;
  if (id == 801 || id == 802) return WeatherCategory::PartlyCloudy;
  if (id == 803 || id == 804) return WeatherCategory::Cloudy;
  return WeatherCategory::Unknown;
}
