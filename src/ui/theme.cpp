#include "theme.h"

#include "config.h"

namespace theme {

namespace {

// {top, mid, bottom, accent}. Tops stay dark so white header text keeps at
// least ~7:1 contrast; tools/preview_screen.py parses these lines.
constexpr Palette kClearDawn = {rgb(34, 30, 96), rgb(214, 86, 140), rgb(255, 158, 92), rgb(255, 220, 120)};
constexpr Palette kClearDay = {rgb(10, 70, 180), rgb(24, 138, 236), rgb(60, 200, 245), rgb(255, 220, 90)};
constexpr Palette kClearDusk = {rgb(40, 30, 100), rgb(232, 84, 74), rgb(255, 176, 64), rgb(255, 220, 120)};
constexpr Palette kClearNight = {rgb(6, 8, 36), rgb(40, 28, 110), rgb(110, 50, 160), rgb(205, 175, 255)};

constexpr Palette kCloudyDawn = {rgb(40, 38, 80), rgb(150, 90, 130), rgb(200, 140, 120), rgb(255, 220, 120)};
constexpr Palette kCloudyDay = {rgb(36, 62, 110), rgb(70, 110, 160), rgb(120, 160, 200), rgb(255, 220, 120)};
constexpr Palette kCloudyDusk = {rgb(44, 38, 80), rgb(160, 90, 90), rgb(210, 150, 100), rgb(255, 220, 120)};
constexpr Palette kCloudyNight = {rgb(10, 12, 30), rgb(36, 38, 72), rgb(70, 64, 110), rgb(205, 175, 255)};

constexpr Palette kRainDawn = {rgb(16, 24, 52), rgb(60, 60, 110), rgb(130, 100, 140), rgb(130, 215, 255)};
constexpr Palette kRainDay = {rgb(14, 28, 58), rgb(28, 76, 120), rgb(40, 150, 170), rgb(130, 215, 255)};
constexpr Palette kRainDusk = {rgb(18, 20, 48), rgb(80, 50, 90), rgb(160, 100, 100), rgb(130, 215, 255)};
constexpr Palette kRainNight = {rgb(6, 10, 26), rgb(18, 34, 64), rgb(30, 70, 100), rgb(130, 215, 255)};

constexpr Palette kStormDawn = {rgb(14, 10, 34), rgb(70, 36, 90), rgb(150, 80, 130), rgb(255, 220, 90)};
constexpr Palette kStormDay = {rgb(14, 10, 34), rgb(56, 36, 104), rgb(150, 70, 170), rgb(255, 220, 90)};
constexpr Palette kStormDusk = {rgb(16, 10, 30), rgb(80, 36, 80), rgb(170, 80, 100), rgb(255, 220, 90)};
constexpr Palette kStormNight = {rgb(8, 6, 22), rgb(34, 22, 66), rgb(80, 40, 110), rgb(255, 220, 90)};

// Rows indexed by DayPhase.
constexpr const Palette* kClear[] = {&kClearDawn, &kClearDay, &kClearDusk, &kClearNight};
constexpr const Palette* kCloudy[] = {&kCloudyDawn, &kCloudyDay, &kCloudyDusk, &kCloudyNight};
constexpr const Palette* kRain[] = {&kRainDawn, &kRainDay, &kRainDusk, &kRainNight};
constexpr const Palette* kStorm[] = {&kStormDawn, &kStormDay, &kStormDusk, &kStormNight};

constexpr int32_t kSkyMidRow = 120;
constexpr int kMinutesPerHour = 60;

constexpr float kHot = 36.0f, kWarm = 33.0f, kCool = 22.0f;
constexpr uint16_t kHotColor = rgb(255, 110, 90);
constexpr uint16_t kWarmColor = rgb(255, 170, 70);
constexpr uint16_t kCoolColor = rgb(140, 210, 255);

DayPhase phaseAt(int minute, int sunrise, int sunset) {
  if (minute >= sunrise - config::kDawnBeforeMin && minute < sunrise + config::kDawnAfterMin) {
    return DayPhase::Dawn;
  }
  if (minute >= sunset - config::kDuskBeforeMin && minute < sunset + config::kDuskAfterMin) {
    return DayPhase::Dusk;
  }
  return minute > sunrise && minute < sunset ? DayPhase::Day : DayPhase::Night;
}

}  // namespace

uint16_t blend(uint8_t alpha, uint16_t fg, uint16_t bg) {
  // Split RGB565 into 5/6/5 channels and blend each one.
  const uint32_t a = alpha, ia = 255 - alpha;
  const uint32_t r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * ia) / 255;
  const uint32_t g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * ia) / 255;
  const uint32_t b = ((fg & 0x1F) * a + (bg & 0x1F) * ia) / 255;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

const Palette& paletteFor(WeatherCategory category, DayPhase phase) {
  const int i = static_cast<int>(phase);
  switch (category) {
    case WeatherCategory::Clear:
    case WeatherCategory::PartlyCloudy:
      return *kClear[i];
    case WeatherCategory::Drizzle:
    case WeatherCategory::Rain:
    case WeatherCategory::Snow:
      return *kRain[i];
    case WeatherCategory::Thunderstorm:
      return *kStorm[i];
    default:  // Cloudy, Fog, Unknown
      return *kCloudy[i];
  }
}

const Palette& paletteFor(const WeatherData* data, const struct tm* now) {
  if (data == nullptr) {
    return defaultPalette();
  }
  DayPhase phase = data->isDay ? DayPhase::Day : DayPhase::Night;
  if (now != nullptr && data->sunriseMin >= 0 && data->sunsetMin > data->sunriseMin) {
    phase = phaseAt(now->tm_hour * kMinutesPerHour + now->tm_min, data->sunriseMin,
                    data->sunsetMin);
  }
  return paletteFor(data->category, phase);
}

const Palette& defaultPalette() { return kCloudyDay; }

uint16_t skyAt(const Palette& p, int16_t y) {
  constexpr int32_t kLastRow = config::kScreenHeight - 1;
  const int32_t row = y < 0 ? 0 : (y > kLastRow ? kLastRow : y);
  if (row <= kSkyMidRow) {
    return blend(static_cast<uint8_t>(row * 255 / kSkyMidRow), p.skyMid, p.skyTop);
  }
  return blend(static_cast<uint8_t>((row - kSkyMidRow) * 255 / (kLastRow - kSkyMidRow)),
               p.skyBottom, p.skyMid);
}

uint16_t glassAt(const Palette& p, int16_t y, uint8_t alpha) {
  return blend(alpha, kGlassTint, skyAt(p, y));
}

uint16_t temperatureColor(float c) {
  if (c >= kHot) return kHotColor;
  if (c >= kWarm) return kWarmColor;
  if (c <= kCool) return kCoolColor;
  return kText;
}

}  // namespace theme
