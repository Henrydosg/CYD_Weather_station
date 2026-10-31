#include "theme.h"

namespace theme {

namespace {
constexpr float kHot = 36.0f, kWarm = 33.0f, kCool = 22.0f;
constexpr uint16_t kHotColor = rgb(255, 110, 90);
constexpr uint16_t kWarmColor = rgb(255, 170, 70);
// UV index: 0-2 low, 3-5 moderate, 6-7 high, 8-10 very high, 11+ extreme.
constexpr float kUvLimits[] = {3, 6, 8, 11};
// US AQI: 0-50 good, -100 moderate, -150 sensitive, -200 unhealthy, -300 very, 301+ hazardous.
constexpr int kAqiLimits[] = {51, 101, 151, 201, 301};
}  // namespace

uint16_t temperatureColor(float c) {
  if (c >= kHot) return kHotColor;
  if (c >= kWarm) return kWarmColor;
  if (c <= kCool) return kLow;
  return kTemp;
}

uint8_t uvLevel(float uv) {
  uint8_t level = 0;
  for (float limit : kUvLimits) {
    if (uv >= limit) {
      ++level;
    }
  }
  return level;
}

uint8_t aqiLevel(int aqi) {
  uint8_t level = 0;
  for (int limit : kAqiLimits) {
    if (aqi >= limit) {
      ++level;
    }
  }
  return level;
}

uint16_t levelColor(uint8_t level) {
  static const uint16_t kColors[] = {kOk, kWarn, kAlert, kError, kSevere, kDanger};
  return kColors[level < 6 ? level : 5];
}

}  // namespace theme
