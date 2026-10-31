// Weather category -> generated icon (72 px, or 30 px "small" for strips).
#pragma once

#include "../weather/weather_data.h"
#include "assets/icons.h"

inline const Icon& weatherIcon(WeatherCategory c, bool isDay, bool small) {
  switch (c) {
    case WeatherCategory::Clear:
      return isDay ? (small ? kIconClearDaySmall : kIconClearDay)
                   : (small ? kIconClearNightSmall : kIconClearNight);
    case WeatherCategory::PartlyCloudy:
      return isDay ? (small ? kIconPartlyDaySmall : kIconPartlyDay)
                   : (small ? kIconPartlyNightSmall : kIconPartlyNight);
    case WeatherCategory::Fog:          return small ? kIconFogSmall : kIconFog;
    case WeatherCategory::Drizzle:      return small ? kIconDrizzleSmall : kIconDrizzle;
    case WeatherCategory::Rain:         return small ? kIconRainSmall : kIconRain;
    case WeatherCategory::Snow:         return small ? kIconSnowSmall : kIconSnow;
    case WeatherCategory::Thunderstorm: return small ? kIconThunderSmall : kIconThunder;
    default:                            return small ? kIconCloudySmall : kIconCloudy;
  }
}
