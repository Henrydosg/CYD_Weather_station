#include "theme.h"

namespace theme {

namespace {
constexpr float kHot = 36.0f, kWarm = 33.0f, kCool = 22.0f;
constexpr uint16_t kHotColor = rgb(220, 38, 38);
constexpr uint16_t kWarmColor = rgb(234, 88, 12);
constexpr uint16_t kCoolColor = rgb(2, 132, 199);
}  // namespace

uint16_t temperatureColor(float c) {
  if (c >= kHot) return kHotColor;
  if (c >= kWarm) return kWarmColor;
  if (c <= kCool) return kCoolColor;
  return kInk;
}

}  // namespace theme
