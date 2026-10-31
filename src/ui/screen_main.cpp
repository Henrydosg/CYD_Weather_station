#include "screen_main.h"

#include <math.h>

#include "assets/fonts.h"
#include "assets/icons.h"
#include "config.h"
#include "draw_utils.h"
#include "icon.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

// Layout (landscape 320 x 240)
constexpr int16_t kMargin = 6;

constexpr int16_t kHeaderH = 28;
constexpr int16_t kHeaderMidY = 14;

// WiFi signal bars (bottom-aligned, right edge of the header)
constexpr int kBarCount = 4;
constexpr int16_t kBarW = 3, kBarGap = 2, kBarMinH = 4, kBarStepH = 3, kBarBottomY = 20;
constexpr int16_t kBarsX = 320 - kMargin - kBarCount * (kBarW + kBarGap) + kBarGap;
constexpr int8_t kRssiLevels[kBarCount] = {-85, -75, -65, -55};  // dBm for 1..4 bars

// Gear and language pill left of the bars
constexpr int16_t kGearSize = 16;
constexpr int16_t kGearX = kBarsX - 10 - kGearSize;
constexpr int16_t kGearY = kHeaderMidY - kGearSize / 2;
constexpr int16_t kPillW = 28, kPillH = 18;
constexpr int16_t kPillX = kGearX - 10 - kPillW;
constexpr int16_t kPillY = kHeaderMidY - kPillH / 2;
// Touch targets are larger than the drawn controls (resistive panel).
constexpr int16_t kHitH = 36;
constexpr int16_t kPillHitX = kPillX - 10;
constexpr int16_t kGearHitX = kGearX - 5;

constexpr int16_t kLeftW = 166;
constexpr int16_t kWeatherY = 30;
constexpr int16_t kWeatherH = 104;
constexpr int16_t kIconX = kMargin + 4;
constexpr int16_t kIconY = kWeatherY + 4;
constexpr int16_t kIconSize = 72;
constexpr int16_t kTempRightX = kMargin + kLeftW - 8;
constexpr int16_t kTempMidY = kIconY + kIconSize / 2;
// The icons have a transparent margin, so the digits may start right after them.
constexpr int16_t kTempMaxW = kTempRightX - (kIconX + kIconSize);
constexpr int16_t kDescX = kMargin + 8;
constexpr int16_t kDescMidY = kWeatherY + kWeatherH - 14;
constexpr int16_t kDescMaxW = kLeftW - 16;

constexpr int16_t kDetailsY = kWeatherY + kWeatherH + 4;
constexpr int16_t kRowH = 20;
constexpr int kRows = 4;
constexpr int16_t kDetailsH = kRowH * kRows + 2;
constexpr int16_t kRowIconX = kMargin + 8;
constexpr int16_t kRowIconSize = 16;
constexpr int16_t kRowLabelX = kRowIconX + kRowIconSize + 8;
constexpr int16_t kRowValueX = kMargin + kLeftW - 8;
constexpr int kHumidityRow = 1;
constexpr int16_t kHumBarW = 30, kHumBarH = 4, kHumBarGap = 6;

constexpr int16_t kClockX = 178;
constexpr int16_t kClockY = 30;
constexpr int16_t kClockSize = 136;
constexpr int16_t kClockMidX = kClockX + kClockSize / 2;

// Card under the clock: digital time (clock sprite) and the date.
constexpr int16_t kTimeCardX = 180;
constexpr int16_t kTimeCardY = kClockY + kClockSize + 2;
constexpr int16_t kTimeCardW = 132;
constexpr int16_t kTimeCardH = 50;
// The digital readout sprite stays clear of the card's rounded corners.
constexpr int16_t kDigitalX = kClockMidX - AnalogClock::kDigitalW / 2;
constexpr int16_t kDigitalY = kTimeCardY + 2;
constexpr int16_t kDateMidY = kTimeCardY + 39;

constexpr int16_t kFooterY = 222;
constexpr int16_t kFooterMidY = kFooterY + 9;

constexpr uint32_t kMinutesPerHour = 60;

const Icon& weatherIcon(WeatherCategory c, bool isDay) {
  switch (c) {
    case WeatherCategory::Clear:        return isDay ? kIconClearDay : kIconClearNight;
    case WeatherCategory::PartlyCloudy: return isDay ? kIconPartlyDay : kIconPartlyNight;
    case WeatherCategory::Cloudy:       return kIconCloudy;
    case WeatherCategory::Fog:          return kIconFog;
    case WeatherCategory::Drizzle:      return kIconDrizzle;
    case WeatherCategory::Rain:         return kIconRain;
    case WeatherCategory::Snow:         return kIconSnow;
    case WeatherCategory::Thunderstorm: return kIconThunder;
    default:                            return kIconCloudy;
  }
}

uint8_t signalBars(WifiState wifi, int8_t rssi) {
  if (wifi != WifiState::Connected) {
    return 0;
  }
  uint8_t bars = 1;  // connected always shows at least one bar
  for (int i = 1; i < kBarCount; ++i) {
    if (rssi >= kRssiLevels[i]) {
      bars = i + 1;
    }
  }
  return bars;
}

}  // namespace

ScreenMain::ScreenMain(TFT_eSPI& tft)
    : tft_(tft), clock_(tft, kClockX, kClockY, kDigitalX, kDigitalY) {}

void ScreenMain::begin() {
  clock_.begin();
  firstFrame_ = true;
}

MainTarget ScreenMain::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, kGearHitX, 0, config::kScreenWidth - kGearHitX, kHitH)) {
    return MainTarget::Settings;
  }
  if (ui::hit(x, y, kPillHitX, 0, kGearHitX - kPillHitX, kHitH)) {
    return MainTarget::Language;
  }
  return MainTarget::None;
}

void ScreenMain::update(const UiState& s) {
  const WeatherData* data = s.dataVersion > 0 ? s.data : nullptr;
  const struct tm* now = s.timeValid ? &s.localTime : nullptr;

  const bool full = firstFrame_;
  if (full) {
    tft_.fillScreen(kBackground);
    clock_.invalidate();
  }

  const bool newData = full || s.dataVersion != last_.dataVersion;
  const uint8_t bars = signalBars(s.wifi, s.rssi);

  if (full || s.wifi != last_.wifi || bars != lastBars_) {
    drawHeader(s.wifi, bars);
    lastBars_ = bars;
  }
  if (newData) {
    // Both shadows before the bodies: each one spills into the gap and onto
    // the neighbouring card.
    ui::drawShadow(tft_, kMargin, kDetailsY, kLeftW, kDetailsH);
    ui::drawShadow(tft_, kMargin, kWeatherY, kLeftW, kWeatherH);
    drawWeather(data);
    drawDetails(data);
  }
  const bool newDate = full || s.timeValid != last_.timeValid ||
                       (s.timeValid && s.localTime.tm_mday != last_.localTime.tm_mday);
  if (newDate) {
    drawDate(now);
  }
  // Last: the card shadows reach into the footer and are repainted there.
  if (newData || newDate || s.fetch != last_.fetch || s.dataAgeMin != last_.dataAgeMin) {
    drawFooter(s.fetch, data, s.dataAgeMin);
  }
  clock_.update(now);

  last_ = s;
  firstFrame_ = false;
}

void ScreenMain::drawHeader(WifiState wifi, uint8_t bars) {
  tft_.fillRect(0, 0, config::kScreenWidth, kHeaderH, kBackground);

  useFont(tft_, kFontMedium);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::LocationName), kMargin + 2, kHeaderMidY);

  // Language pill shows the active language; tapping it toggles.
  tft_.fillSmoothRoundRect(kPillX, kPillY, kPillW, kPillH, kPillH / 2, kAccent, kBackground);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kOnAccent, kAccent);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(currentLanguage() == Language::English ? "EN" : "VI", kPillX + kPillW / 2,
                  kHeaderMidY);

  drawIcon(tft_, kIconGear, kGearX, kGearY, kBackground);

  // Signal bars: green by strength; one yellow/red bar while (re)connecting.
  uint16_t on = kOk;
  if (wifi == WifiState::Connecting) {
    on = kWarn;
    bars = 1;
  } else if (wifi != WifiState::Connected) {
    on = kError;
    bars = 1;
  }
  for (int i = 0; i < kBarCount; ++i) {
    const int16_t h = kBarMinH + i * kBarStepH;
    const int16_t x = kBarsX + i * (kBarW + kBarGap);
    tft_.fillRect(x, kBarBottomY - h, kBarW, h, i < bars ? on : kBarOff);
  }
}

void ScreenMain::drawWeather(const WeatherData* d) {
  ui::drawCardBody(tft_, kMargin, kWeatherY, kLeftW, kWeatherH);

  if (d == nullptr) {
    useFont(tft_, kFontSmall);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(tr(Str::WaitingForData), kMargin + kLeftW / 2, kWeatherY + kWeatherH / 2);
    return;
  }

  drawIcon(tft_, weatherIcon(d->category, d->isDay), kIconX, kIconY, kBackground);

  char buf[16];
  snprintf(buf, sizeof(buf), "%ld°", lroundf(d->temperatureC));
  useFont(tft_, kFontTemp);
  if (tft_.textWidth(buf) > kTempMaxW) {
    useFont(tft_, kFontMedium);
  }
  tft_.setTextColor(temperatureColor(d->temperatureC), kBackground);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(buf, kTempRightX, kTempMidY);

  tft_.setTextColor(kAccent, kBackground);
  tft_.setTextDatum(ML_DATUM);
  ui::drawFitted(tft_, d->description, kDescX, kDescMidY, kDescMaxW, kFontMedium, kFontSmall);
}

void ScreenMain::drawDetails(const WeatherData* d) {
  ui::drawCardBody(tft_, kMargin, kDetailsY, kLeftW, kDetailsH);

  char values[kRows][16];
  uint16_t valueColor[kRows] = {kInk, kInk, kInk, kInk};
  if (d != nullptr) {
    snprintf(values[0], sizeof(values[0]), "%ld°", lroundf(d->apparentC));
    valueColor[0] = temperatureColor(d->apparentC);
    snprintf(values[1], sizeof(values[1]), "%.0f%%", d->humidityPct);
    snprintf(values[2], sizeof(values[2]), "%.0f km/h", d->windKmh);
    snprintf(values[3], sizeof(values[3]), "%.0f hPa", d->pressureHpa);
  } else {
    for (auto& v : values) {
      strlcpy(v, "--", sizeof(v));
    }
  }
  const Icon* icons[kRows] = {&kIconFeels, &kIconHumidity, &kIconWind, &kIconPressure};
  const Str labels[kRows] = {Str::FeelsLike, Str::Humidity, Str::Wind, Str::Pressure};

  useFont(tft_, kFontSmall);
  int16_t humLabelEnd = kRowLabelX;
  for (int i = 0; i < kRows; ++i) {
    const int16_t top = kDetailsY + 1 + i * kRowH;
    const int16_t mid = top + kRowH / 2;
    if (i > 0) {
      tft_.drawFastHLine(kRowIconX, top, kLeftW - 16, kLine);
    }
    drawIcon(tft_, *icons[i], kRowIconX, mid - kRowIconSize / 2, kBackground);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(ML_DATUM);
    const int16_t w = tft_.drawString(tr(labels[i]), kRowLabelX, mid);
    if (i == kHumidityRow) {
      humLabelEnd = kRowLabelX + w;
    }
    tft_.setTextColor(valueColor[i], kBackground);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(values[i], kRowValueX, mid);
  }

  // Humidity bar right after the label ("Độ ẩm" / "Humidity" differ in width).
  const int16_t barX = humLabelEnd + kHumBarGap;
  const int16_t barY = kDetailsY + 1 + kHumidityRow * kRowH + (kRowH - kHumBarH) / 2;
  tft_.fillSmoothRoundRect(barX, barY, kHumBarW, kHumBarH, kHumBarH / 2, kTrack, kBackground);
  if (d != nullptr) {
    const float pct = constrain(d->humidityPct, 0.0f, 100.0f);
    const int16_t w = max<int16_t>(kHumBarH, lroundf(kHumBarW * pct / 100.0f));
    tft_.fillSmoothRoundRect(barX, barY, w, kHumBarH, kHumBarH / 2, kAccent, kBackground);
  }
}

void ScreenMain::drawFooter(FetchStatus fetch, const WeatherData* d, uint32_t ageMin) {
  constexpr int16_t kFooterH = config::kScreenHeight - kFooterY;
  tft_.fillRect(0, kFooterY, config::kScreenWidth, kFooterH, kBackground);
  // Restore the bottom of the card shadows, clipped to the footer.
  tft_.setViewport(0, kFooterY, config::kScreenWidth, kFooterH, false);
  ui::drawShadow(tft_, kMargin, kDetailsY, kLeftW, kDetailsH);
  ui::drawShadow(tft_, kTimeCardX, kTimeCardY, kTimeCardW, kTimeCardH);
  tft_.resetViewport();
  useFont(tft_, kFontSmall);

  char buf[32];
  if (d == nullptr) {
    strlcpy(buf, tr(Str::NoDataYet), sizeof(buf));
  } else if (ageMin == 0) {
    strlcpy(buf, tr(Str::JustUpdated), sizeof(buf));
  } else if (ageMin < kMinutesPerHour) {
    snprintf(buf, sizeof(buf), tr(Str::MinutesAgoFmt), static_cast<unsigned long>(ageMin));
  } else {
    snprintf(buf, sizeof(buf), tr(Str::HoursAgoFmt),
             static_cast<unsigned long>(ageMin / kMinutesPerHour));
  }
  tft_.setTextColor(kMuted, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(buf, kMargin + 2, kFooterMidY);

  Str msg = Str::TapToRefresh;
  uint16_t color = kMuted;
  switch (fetch) {
    case FetchStatus::Fetching: msg = Str::Updating;     color = kWarn;  break;
    case FetchStatus::Error:    msg = Str::UpdateFailed; color = kError; break;
    default: break;
  }
  tft_.setTextColor(color, kBackground);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(tr(msg), config::kScreenWidth - kMargin - 2, kFooterMidY);
}

void ScreenMain::drawDate(const struct tm* t) {
  // Repaints the whole time card (once a day); the clock sprite redraws the
  // digital readout in the same loop iteration.
  ui::drawCard(tft_, kTimeCardX, kTimeCardY, kTimeCardW, kTimeCardH);
  clock_.invalidate();
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kAccent, kBackground);
  tft_.setTextDatum(MC_DATUM);
  char buf[40];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), tr(Str::DateFmt), weekdayName(t->tm_wday), t->tm_mday,
             t->tm_mon + 1);
  } else {
    strlcpy(buf, tr(Str::SyncingTime), sizeof(buf));
  }
  tft_.drawString(buf, kClockMidX, kDateMidY);
}
