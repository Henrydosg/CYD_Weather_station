// Rain outlook from the 15-minute precipitation nowcast (weather_data.h).
#include "weather_data.h"

namespace {

constexpr uint32_t kSlotSec = 15 * 60;
constexpr float kTenthsPerMm = 10.0f;

bool wet(const CityWeather& w, int slot) {
  return w.nowcastTenthsMm[slot] >= config::kRainMinMm * kTenthsPerMm;
}

}  // namespace

RainOutlook rainOutlook(const CityWeather& w, uint32_t utcNow) {
  RainOutlook out{RainKind::None, 0};
  const uint32_t start = w.nowcastStartUtc;
  if (!w.valid || w.nowcastCount == 0 || utcNow < start ||
      utcNow - start >= w.nowcastCount * kSlotSec) {
    return out;  // no data, or all of it in the past
  }
  const int now = static_cast<int>((utcNow - start) / kSlotSec);
  const bool raining = w.precipitationMm >= config::kRainMinMm || wet(w, now);
  for (int i = now + 1; i < w.nowcastCount; ++i) {
    if (wet(w, i) != raining) {
      out.kind = raining ? RainKind::Raining : RainKind::Soon;
      out.atUtc = start + i * kSlotSec;
      return out;
    }
  }
  if (raining) {
    out.kind = RainKind::Continues;
  }
  return out;
}
