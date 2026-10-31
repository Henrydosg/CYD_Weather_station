#include "theme.h"

#include "config.h"

namespace theme {

namespace {

constexpr Palette kClearDay = {rgb(56, 136, 214), rgb(22, 70, 140), rgb(255, 214, 102)};
constexpr Palette kCloudyDay = {rgb(58, 110, 170), rgb(20, 44, 84), rgb(255, 214, 102)};
constexpr Palette kFogDay = {rgb(96, 112, 134), rgb(36, 46, 62), rgb(230, 236, 245)};
constexpr Palette kRainDay = {rgb(52, 78, 118), rgb(14, 24, 44), rgb(130, 205, 255)};
constexpr Palette kStormDay = {rgb(62, 58, 98), rgb(16, 14, 34), rgb(255, 214, 102)};
constexpr Palette kNight = {rgb(40, 34, 92), rgb(8, 10, 30), rgb(199, 170, 255)};
constexpr Palette kRainNight = {rgb(30, 40, 78), rgb(6, 10, 24), rgb(130, 205, 255)};

constexpr float kHot = 36.0f, kWarm = 33.0f, kCool = 22.0f;
constexpr uint16_t kHotColor = rgb(255, 110, 90);
constexpr uint16_t kWarmColor = rgb(255, 170, 70);
constexpr uint16_t kCoolColor = rgb(140, 210, 255);

}  // namespace

uint16_t blend(uint8_t alpha, uint16_t fg, uint16_t bg) {
  // Split RGB565 into 5/6/5 channels and blend each one.
  const uint32_t a = alpha, ia = 255 - alpha;
  const uint32_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * ia) / 255;
  const uint32_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * ia) / 255;
  const uint32_t b = ((fg & 0x1F) * a + (bg & 0x1F) * ia) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

const Palette& paletteFor(WeatherCategory category, bool isDay) {
  switch (category) {
    case WeatherCategory::Drizzle:
    case WeatherCategory::Rain:
    case WeatherCategory::Snow:
      return isDay ? kRainDay : kRainNight;
    case WeatherCategory::Thunderstorm:
      return isDay ? kStormDay : kRainNight;
    case WeatherCategory::Fog:
      return isDay ? kFogDay : kNight;
    case WeatherCategory::Clear:
      return isDay ? kClearDay : kNight;
    default:
      return isDay ? kCloudyDay : kNight;
  }
}

const Palette& defaultPalette() { return kCloudyDay; }

uint16_t skyAt(const Palette& p, int16_t y) {
  constexpr int32_t kLastRow = config::kScreenHeight - 1;
  const int32_t row = y < 0 ? 0 : (y > kLastRow ? kLastRow : y);
  return blend(static_cast<uint8_t>(255 - row * 255 / kLastRow), p.skyTop, p.skyBottom);
}

uint16_t temperatureColor(float c) {
  if (c >= kHot) return kHotColor;
  if (c >= kWarm) return kWarmColor;
  if (c <= kCool) return kCoolColor;
  return kText;
}

}  // namespace theme
