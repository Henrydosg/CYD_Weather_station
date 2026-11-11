#include "screen_graph.h"

#include <math.h>

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "icon.h"
#include "layout.h"
#include "strings.h"
#include "weather_icons.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;

constexpr int16_t kNameX = ui::lay::X(8), kNameMidY = ui::lay::Y(12), kNameMaxW = ui::lay::X(196);
constexpr int16_t kSubMidY = ui::lay::Y(31);
constexpr int16_t kTimeRightX = ui::lay::X(312), kTimeMidY = ui::lay::Y(12);
constexpr int16_t kRuleX = ui::lay::X(6), kRuleY = ui::lay::Y(40);

// Plot: first point x, spacing, temperature range mapped to y
constexpr int16_t kPlotX0 = ui::lay::X(22), kPlotDx = ui::lay::X(39);
constexpr int16_t kPlotTop = ui::lay::Y(108), kPlotBottom = ui::lay::Y(176);
constexpr float kLineR = ui::lay::Sf(1.0f);
constexpr int16_t kPointR = ui::lay::S(3);
constexpr int16_t kIconSize = ui::lay::S(30);
constexpr int16_t kIconGap = ui::lay::Y(6);  // icon bottom sits kIconGap above the point
constexpr int16_t kTempAbove = ui::lay::Y(44), kHourAbove = ui::lay::Y(57);

// Rain chance
constexpr int16_t kRainRuleY = ui::lay::Y(194), kRainLabelY = ui::lay::Y(202);
constexpr int16_t kRainBarBottom = ui::lay::Y(226);
constexpr int16_t kRainBarMaxH = ui::lay::Y(30);
constexpr int16_t kRainBarHalfW = ui::lay::X(6);
constexpr int16_t kRainTextY = ui::lay::Y(234);
constexpr int kPercent = 100;

const CityWeather* weatherOf(const UiState& s, uint8_t city) {
  return s.cities != nullptr && city < s.cities->count && s.weather != nullptr
             ? &s.weather[city]
             : nullptr;
}

}  // namespace

void ScreenGraph::show(const UiState& s, uint8_t city) {
  city_ = city;
  lastData_ = s.dataVersion;
  tft_.fillScreen(kBackground);
  drawHeader(s);
  const CityWeather* w = weatherOf(s, city_);
  if (w != nullptr && w->valid && w->hourlyCount > 0) {
    drawPlot(*w);
  }
}

void ScreenGraph::update(const UiState& s) {
  if (s.dataVersion != lastData_) {
    show(s, city_);
    return;
  }
  const CityWeather* w = weatherOf(s, city_);
  int minute = -1;
  if (s.timeValid && w != nullptr && w->valid) {
    const time_t t = s.utcNow + w->utcOffsetSec;
    struct tm tm;
    gmtime_r(&t, &tm);
    minute = tm.tm_min;
  }
  if (minute != lastMinute_) {
    drawHeader(s);
  }
}

void ScreenGraph::drawHeader(const UiState& s) {
  tft_.fillRect(0, 0, kW, kRuleY, kBackground);
  const bool hasCity = s.cities != nullptr && city_ < s.cities->count;
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kInk, kBackground);
  ui::drawFitted(tft_, hasCity ? cityName(s.cities->items[city_], currentLanguage()) : "--",
                 kNameX, kNameMidY, kNameMaxW, kFontMedium, kFontSmall);
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(tr(Str::Next24h), kNameX, kSubMidY);

  const CityWeather* w = weatherOf(s, city_);
  char buf[8] = "--:--";
  lastMinute_ = -1;
  if (s.timeValid && w != nullptr && w->valid) {
    const time_t t = s.utcNow + w->utcOffsetSec;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    lastMinute_ = tm.tm_min;
  }
  useFont(tft_, kFontMedium);
  tft_.setTextColor(kHighlight, kBackground);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(buf, kTimeRightX, kTimeMidY);
  tft_.drawFastHLine(kRuleX, kRuleY, kW - 2 * kRuleX, kLine);
}

void ScreenGraph::drawPlot(const CityWeather& w) {
  const int n = w.hourlyCount;
  float lo = w.hourly[0].tempC, hi = lo;
  for (int i = 1; i < n; ++i) {
    lo = fminf(lo, w.hourly[i].tempC);
    hi = fmaxf(hi, w.hourly[i].tempC);
  }
  const float span = fmaxf(hi - lo, 1.0f);
  int16_t xs[config::kForecastHours / config::kHourlyStep];
  int16_t ys[config::kForecastHours / config::kHourlyStep];
  for (int i = 0; i < n; ++i) {
    xs[i] = kPlotX0 + i * kPlotDx;
    ys[i] = kPlotBottom - lroundf((w.hourly[i].tempC - lo) * (kPlotBottom - kPlotTop) / span);
  }

  for (int i = 1; i < n; ++i) {
    tft_.drawWedgeLine(xs[i - 1], ys[i - 1], xs[i], ys[i], kLineR, kLineR, kGraph, kBackground);
  }
  char buf[12];
  for (int i = 0; i < n; ++i) {
    const HourlyPoint& p = w.hourly[i];
    tft_.fillSmoothCircle(xs[i], ys[i], kPointR, kGraph, kBackground);
    drawIcon(tft_, weatherIcon(wmoCategory(p.wmoCode), p.isDay, true), xs[i] - kIconSize / 2,
             ys[i] - kIconGap - kIconSize, kBackground);
    tft_.setTextDatum(MC_DATUM);
    useFont(tft_, kFontTinyBold);
    snprintf(buf, sizeof(buf), "%ld°", lroundf(p.tempC));
    tft_.setTextColor(kTemp, kBackground);
    tft_.drawString(buf, xs[i], ys[i] - kTempAbove);
    useFont(tft_, kFontTiny);
    snprintf(buf, sizeof(buf), tr(Str::HourFmt), p.hour);
    tft_.setTextColor(kMuted, kBackground);
    tft_.drawString(buf, xs[i], ys[i] - kHourAbove);

    // Chance of rain
    const int16_t barH = p.precipPct * kRainBarMaxH / kPercent;
    if (barH > 0) {
      tft_.fillRect(xs[i] - kRainBarHalfW, kRainBarBottom - barH, 2 * kRainBarHalfW, barH,
                    kRainBar);
    }
    snprintf(buf, sizeof(buf), "%u%%", p.precipPct);
    tft_.setTextColor(kLow, kBackground);
    tft_.drawString(buf, xs[i], kRainTextY);
  }
  tft_.drawFastHLine(kRuleX, kRainRuleY, kW - 2 * kRuleX, kLine);
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kMuted, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::RainChance), kNameX, kRainLabelY);
}
