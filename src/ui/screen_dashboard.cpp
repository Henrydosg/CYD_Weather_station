#include "screen_dashboard.h"

#include <math.h>

#include "assets/fonts.h"
#include "assets/icons.h"
#include "config.h"
#include "draw_utils.h"
#include "icon.h"
#include "strings.h"
#include "weather_icons.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kMinutesPerHour = 60;

// Header
constexpr int16_t kNameX = 8, kNameMidY = 12, kNameMaxW = 196;
constexpr int16_t kRegionMidY = 31, kRegionMaxW = 170;
constexpr int16_t kTimeRightX = 288, kTimeMidY = 12;
constexpr int16_t kBarsX = 294, kBarsBottom = 18;
constexpr int16_t kDateRightX = 312, kDateMidY = 31;
constexpr int16_t kLunarGap = 8;   // between the date and the lunar date
constexpr int16_t kRegionGap = 6;  // between the region and the date
constexpr int16_t kHeaderH = 40;  // also the tap area for Settings
constexpr int16_t kRuleY = 40;
constexpr int16_t kDotPitch = 10, kDotR = 2, kDotActiveR = 3, kDotsPad = 6;

// Current weather (left)
constexpr int16_t kLeftW = 150;
constexpr int16_t kBodyY = 42, kBodyH = 114;
constexpr int16_t kDescMidX = 76, kDescMidY = 54, kDescMaxW = 148;
constexpr int16_t kIconX = 2, kIconY = 66;
constexpr int16_t kTempMidX = 112, kTempMidY = 98;
constexpr int16_t kFeelsMidY = 130;
// Note line (rain / lunar), with a rain drop before rain notes
constexpr int16_t kNoteTop = 140, kNoteH = 16, kNoteMidY = 147;
constexpr int16_t kDropW = 12, kDropR = 4, kDropTip = 6;
constexpr uint32_t kSecondsPerMinute = 60;
constexpr uint32_t kRoundMinutes = 5;  // "~25 min"
constexpr int kFullMoonDay = 15;

// Details (right)
constexpr int16_t kColonX = 230;
constexpr int16_t kDetailsY = 50, kDetailsPitch = 14;
constexpr int kDetailRows = 8;

// 7-day strip
constexpr int16_t kStripRuleY = 158;
constexpr int16_t kStripX = 6, kStripW = kW - 12;
constexpr int16_t kTodayTop = 161, kTodayH = 77, kTodayRadius = 6;
constexpr int16_t kDayNameY = 169, kDayDateY = 182, kDayIconY = 189, kDayHighY = 222,
                  kDayLowY = 234;
constexpr int16_t kSmallIcon = 30;

const CityWeather* current(const UiState& s, uint8_t city) {
  return s.cities != nullptr && city < s.cities->count && s.weather != nullptr
             ? &s.weather[city]
             : nullptr;
}

// City local time as broken-down UTC fields.
bool cityTime(const UiState& s, const CityWeather* w, struct tm& out) {
  if (!s.timeValid || w == nullptr || !w->valid) {
    return false;
  }
  const time_t t = s.utcNow + w->utcOffsetSec;
  gmtime_r(&t, &out);
  return true;
}

// Local wall-clock time of a UTC instant in a city.
struct tm localTime(uint32_t utc, int32_t offsetSec) {
  const time_t t = static_cast<time_t>(utc) + offsetSec;
  struct tm out;
  gmtime_r(&t, &out);
  return out;
}

void formatMinutes(char* buf, size_t size, int16_t minutes) {
  if (minutes < 0) {
    strlcpy(buf, "--:--", size);
  } else {
    snprintf(buf, size, "%02d:%02d", minutes / kMinutesPerHour, minutes % kMinutesPerHour);
  }
}

}  // namespace

void ScreenDashboard::pause(uint32_t now) {
  paused_ = true;
  pausedAt_ = now;
}

DashTarget ScreenDashboard::hitTest(int16_t, int16_t y) const {
  return y < kHeaderH ? DashTarget::Settings : DashTarget::Graph;
}

void ScreenDashboard::show(const UiState& s, uint32_t now) {
  tft_.fillScreen(kBackground);
  redraw(s, now);
}

// Every part clears only its own area, so a city change or new data never
// blanks the whole screen (no visible flash).
void ScreenDashboard::redraw(const UiState& s, uint32_t now) {
  const uint8_t n = s.cities != nullptr ? s.cities->count : 0;
  if (city_ >= n) {
    city_ = 0;
  }
  shownAt_ = now;
  lastData_ = s.dataVersion;
  lastCities_ = s.citiesVersion;
  lastWifi_ = s.wifi;
  lastBars_ = s.wifi == WifiState::Connected ? ui::signalBars(s.rssi) : 0;

  const CityWeather* w = current(s, city_);
  if (w == nullptr || !w->valid) {
    drawWaiting(s);
  } else {
    drawCurrent(*w);
    drawNote(s, true);
    drawDetails(*w);
    drawDays(*w);
  }
  drawHeader(s);  // last: the position dots overlap the top of the body
}

void ScreenDashboard::update(const UiState& s, uint32_t now) {
  const uint8_t n = s.cities != nullptr ? s.cities->count : 0;

  if (paused_ && now - pausedAt_ >= config::kRotatePauseMs) {
    paused_ = false;
    shownAt_ = now;
  }
  const bool rotate = !paused_ && n > 1 &&
                      now - shownAt_ >= static_cast<uint32_t>(s.rotateSec) * 1000UL;
  if (rotate) {
    city_ = (city_ + 1) % n;
  }
  if (rotate || s.dataVersion != lastData_ || s.citiesVersion != lastCities_) {
    redraw(s, now);
    return;
  }

  // Header: city local minute, WiFi signal
  struct tm t;
  const int minute = cityTime(s, current(s, city_), t) ? t.tm_min : -1;
  const uint8_t bars = s.wifi == WifiState::Connected ? ui::signalBars(s.rssi) : 0;
  if (minute != lastMinute_ || s.wifi != lastWifi_ || bars != lastBars_) {
    lastWifi_ = s.wifi;
    lastBars_ = bars;
    drawHeader(s);
    drawNote(s, false);  // rain minutes, the day of the lunar month
  }
  // Blinking ":" shows the clock (and the firmware) is running.
  const bool colonOn = s.utcNow % 2 == 0;
  if (minute >= 0 && colonOn != colonOn_) {
    drawColon(colonOn);
  }
}

void ScreenDashboard::drawColon(bool on) {
  colonOn_ = on;
  char tail[4];  // ":MM", right of the colon's left edge
  snprintf(tail, sizeof(tail), ":%02d", lastMinute_);
  useFont(tft_, kFontMedium);
  const int16_t x = kTimeRightX - tft_.textWidth(tail);
  // Drawing the glyph in the background colour erases it exactly.
  tft_.setTextColor(on ? kHighlight : kBackground, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(":", x, kTimeMidY);
}

void ScreenDashboard::drawHeader(const UiState& s) {
  tft_.fillRect(0, 0, kW, kRuleY, kBackground);
  const bool hasCity = s.cities != nullptr && city_ < s.cities->count;
  const City* c = hasCity ? &s.cities->items[city_] : nullptr;
  const CityWeather* w = current(s, city_);

  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kInk, kBackground);
  const Language lang = currentLanguage();
  ui::drawFitted(tft_, c != nullptr ? cityName(*c, lang) : "--", kNameX, kNameMidY, kNameMaxW,
                 kFontMedium, kFontSmall);

  // City local time and date
  struct tm t;
  char buf[24];
  const bool timeOk = cityTime(s, w, t);
  int16_t regionMaxW = kRegionMaxW;
  lastMinute_ = timeOk ? t.tm_min : -1;
  colonOn_ = true;  // the full time string below includes the ":"
  if (timeOk) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  } else {
    strlcpy(buf, "--:--", sizeof(buf));
  }
  useFont(tft_, kFontMedium);
  tft_.setTextColor(kHighlight, kBackground);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(buf, kTimeRightX, kTimeMidY);
  if (timeOk) {
    // "T7, 01/11" in yellow, then the lunar date "ÂL 23/9" in the label colour
    updateLunar(t);
    char lunar[24];
    snprintf(lunar, sizeof(lunar), tr(Str::LunarFmt), lunar_.day, lunar_.month,
             lunar_.leap ? tr(Str::LunarLeap) : "");
    snprintf(buf, sizeof(buf), "%s, %02d/%02d", weekdayShort(t.tm_wday), t.tm_mday,
             t.tm_mon + 1);
    useFont(tft_, kFontTiny);
    tft_.setTextColor(kLabel, kBackground);
    tft_.drawString(lunar, kDateRightX, kDateMidY);
    const int16_t dateRight = kDateRightX - tft_.textWidth(lunar) - kLunarGap;
    tft_.setTextColor(kHighlight, kBackground);
    tft_.drawString(buf, dateRight, kDateMidY);
    const int16_t dateLeft = dateRight - tft_.textWidth(buf);
    if (dateLeft - kRegionGap - kNameX < regionMaxW) {
      regionMaxW = dateLeft - kRegionGap - kNameX;
    }
  }
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kMuted, kBackground);
  ui::drawFitted(tft_, c != nullptr ? cityRegion(*c, lang) : "", kNameX, kRegionMidY,
                 regionMaxW, kFontTiny, kFontTiny);

  // Signal: green bars, one yellow / red bar while (re)connecting.
  uint16_t on = kOk;
  uint8_t lit = s.wifi == WifiState::Connected ? ui::signalBars(s.rssi) : 1;
  if (s.wifi == WifiState::Connecting) {
    on = kWarn;
  } else if (s.wifi != WifiState::Connected) {
    on = kError;
  }
  ui::drawSignalBars(tft_, kBarsX, kBarsBottom, lit, on);

  // Rule with one dot per city (the current one highlighted)
  tft_.drawFastHLine(kStripX, kRuleY, kStripW, kLine);
  const uint8_t n = s.cities != nullptr ? s.cities->count : 0;
  if (n > 1) {
    const int16_t x0 = kW / 2 - (n - 1) * kDotPitch / 2;
    tft_.fillRect(x0 - kDotsPad, kRuleY - kDotActiveR, (n - 1) * kDotPitch + 2 * kDotsPad,
                  2 * kDotActiveR + 1, kBackground);
    for (int i = 0; i < n; ++i) {
      const bool active = i == city_;
      tft_.fillSmoothCircle(x0 + i * kDotPitch, kRuleY, active ? kDotActiveR : kDotR,
                            active ? kHighlight : kCardBorder, kBackground);
    }
  }
}

void ScreenDashboard::drawWaiting(const UiState& s) {
  tft_.fillRect(0, kBodyY, kW, config::kScreenHeight - kBodyY, kBackground);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(s.fetch == FetchStatus::Error ? kError : kMuted, kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(tr(Str::WaitingForData), kW / 2, (kRuleY + config::kScreenHeight) / 2);
}

void ScreenDashboard::drawCurrent(const CityWeather& w) {
  tft_.fillRect(0, kBodyY, kLeftW, kBodyH, kBackground);
  tft_.setTextColor(kDescription, kBackground);
  tft_.setTextDatum(MC_DATUM);
  ui::drawFitted(tft_, weatherText(w.wmoCode), kDescMidX, kDescMidY, kDescMaxW, kFontMedium,
                 kFontSmall);

  drawIcon(tft_, weatherIcon(wmoCategory(w.wmoCode), w.isDay, false), kIconX, kIconY,
           kBackground);

  char buf[24];
  snprintf(buf, sizeof(buf), "%ld°", lroundf(w.temperatureC));
  useFont(tft_, kFontBig);
  tft_.setTextColor(temperatureColor(w.temperatureC), kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(buf, kTempMidX, kTempMidY);

  snprintf(buf, sizeof(buf), tr(Str::FeelsFmt), lroundf(w.apparentC));
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kFeels, kBackground);
  tft_.drawString(buf, kTempMidX, kFeelsMidY);
}

void ScreenDashboard::updateLunar(const struct tm& t) {
  const int32_t key = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
  if (key == lunarKey_) {
    return;
  }
  lunarKey_ = key;
  lunar_ = solarToLunar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  tetDays_ = daysToTet(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

void ScreenDashboard::drawNote(const UiState& s, bool force) {
  char text[sizeof(lastNote_)] = "";
  bool rain = false;
  const CityWeather* w = current(s, city_);
  struct tm t;
  if (cityTime(s, w, t)) {
    const uint32_t now = static_cast<uint32_t>(s.utcNow);
    const RainOutlook outlook = rainOutlook(*w, now);
    const struct tm at = localTime(outlook.atUtc, w->utcOffsetSec);
    rain = outlook.kind != RainKind::None;
    switch (outlook.kind) {
      case RainKind::Soon: {
        const uint32_t minutes =
            (outlook.atUtc - now + kSecondsPerMinute - 1) / kSecondsPerMinute;
        if (minutes < static_cast<uint32_t>(kMinutesPerHour)) {
          uint32_t rounded = (minutes + kRoundMinutes - 1) / kRoundMinutes * kRoundMinutes;
          if (rounded < kRoundMinutes) {
            rounded = kRoundMinutes;
          }
          snprintf(text, sizeof(text), tr(Str::RainSoonFmt), static_cast<unsigned>(rounded));
        } else {
          snprintf(text, sizeof(text), tr(Str::RainAtFmt), at.tm_hour, at.tm_min);
        }
        break;
      }
      case RainKind::Raining:
        snprintf(text, sizeof(text), tr(Str::RainStopsFmt), at.tm_hour, at.tm_min);
        break;
      case RainKind::Continues:
        strlcpy(text, tr(Str::RainContinues), sizeof(text));
        break;
      case RainKind::None:
        updateLunar(t);
        if (lunar_.day == 1 && lunar_.month == 1 && !lunar_.leap) {
          strlcpy(text, tr(Str::TetToday), sizeof(text));
        } else if (lunar_.day == kFullMoonDay) {
          snprintf(text, sizeof(text), tr(Str::FullMoonFmt), lunar_.month);
        } else if (lunar_.day == 1) {
          snprintf(text, sizeof(text), tr(Str::NewMoonFmt), lunar_.month);
        } else if (tetDays_ > 0) {
          snprintf(text, sizeof(text), tr(Str::TetCountdownFmt), tetDays_);
        }
        break;
    }
  }
  if (!force && strcmp(text, lastNote_) == 0) {
    return;
  }
  strlcpy(lastNote_, text, sizeof(lastNote_));
  tft_.fillRect(0, kNoteTop, kLeftW, kNoteH, kBackground);
  if (text[0] == '\0') {
    return;
  }
  useFont(tft_, kFontTiny);
  const uint16_t color = rain ? kDescription : kFeels;
  const int16_t textW = tft_.textWidth(text);
  int16_t x = kDescMidX - (textW + (rain ? kDropW : 0)) / 2;
  if (rain) {
    const int16_t cx = x + kDropR;
    tft_.fillTriangle(cx - kDropR + 1, kNoteMidY, cx + kDropR - 1, kNoteMidY, cx,
                      kNoteMidY - kDropTip, color);
    tft_.fillSmoothCircle(cx, kNoteMidY + 1, kDropR, color, kBackground);
    x += kDropW;
  }
  tft_.setTextColor(color, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(text, x, kNoteMidY);
}

void ScreenDashboard::drawDetails(const CityWeather& w) {
  tft_.fillRect(kLeftW, kBodyY, kW - kLeftW, kBodyH, kBackground);
  char values[kDetailRows][24];
  uint16_t colors[kDetailRows];
  for (auto& c : colors) {
    c = kInk;
  }
  formatMinutes(values[0], sizeof(values[0]), w.sunriseMin);
  formatMinutes(values[1], sizeof(values[1]), w.sunsetMin);
  snprintf(values[2], sizeof(values[2]), "%.0f%%", w.humidityPct);
  snprintf(values[3], sizeof(values[3]), "%.0f km/h %s", w.windKmh, compassName(w.windDirDeg));
  snprintf(values[4], sizeof(values[4]), "%.0f hPa", w.pressureHpa);
  snprintf(values[5], sizeof(values[5]), "%u%%", w.cloudPct);
  const uint8_t uv = uvLevel(w.uvIndex);
  snprintf(values[6], sizeof(values[6]), "%.0f %s", w.uvIndex, uvLevelName(uv));
  colors[6] = levelColor(uv);
  if (w.usAqi >= 0) {
    const uint8_t aqi = aqiLevel(w.usAqi);
    snprintf(values[7], sizeof(values[7]), "%d %s", w.usAqi, aqiLevelName(aqi));
    colors[7] = levelColor(aqi);
  } else {
    strlcpy(values[7], "--", sizeof(values[7]));
  }
  const Str labels[kDetailRows] = {Str::Sunrise, Str::Sunset,   Str::Humidity, Str::Wind,
                                   Str::Pressure, Str::Clouds, Str::UvIndex,  Str::Aqi};

  useFont(tft_, kFontLabel);
  char label[32];
  for (int i = 0; i < kDetailRows; ++i) {
    const int16_t y = kDetailsY + i * kDetailsPitch;
    snprintf(label, sizeof(label), "%s:", tr(labels[i]));
    tft_.setTextColor(kLabel, kBackground);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(label, kColonX, y);
    tft_.setTextColor(colors[i], kBackground);
    tft_.setTextDatum(ML_DATUM);
    tft_.drawString(values[i], kColonX + 4, y);
  }
}

void ScreenDashboard::drawDays(const CityWeather& w) {
  tft_.fillRect(0, kStripRuleY, kW, config::kScreenHeight - kStripRuleY, kBackground);
  tft_.drawFastHLine(kStripX, kStripRuleY, kStripW, kLine);
  const int16_t colW = kStripW / config::kForecastDays;
  char buf[12];
  for (int i = 0; i < w.dailyCount; ++i) {
    const DailyForecast& d = w.daily[i];
    const int16_t x = kStripX + i * colW;
    const int16_t cx = x + colW / 2;
    const uint16_t bg = i == 0 ? kTrack : kBackground;  // today
    if (i == 0) {
      tft_.fillSmoothRoundRect(x + 1, kTodayTop, colW - 2, kTodayH, kTodayRadius, kTrack,
                               kBackground);
    }
    tft_.setTextDatum(MC_DATUM);
    useFont(tft_, kFontTinyBold);
    tft_.setTextColor(kInk, bg);
    tft_.drawString(weekdayShort(d.wday), cx, kDayNameY);
    snprintf(buf, sizeof(buf), "%u/%u", d.mday, d.month);
    useFont(tft_, kFontTiny);
    tft_.setTextColor(kMuted, bg);
    tft_.drawString(buf, cx, kDayDateY);
    drawIcon(tft_, weatherIcon(wmoCategory(d.wmoCode), true, true), cx - kSmallIcon / 2,
             kDayIconY, bg);
    useFont(tft_, kFontTinyBold);
    snprintf(buf, sizeof(buf), "%ld°", lroundf(d.maxC));
    tft_.setTextColor(kHigh, bg);
    tft_.drawString(buf, cx, kDayHighY);
    snprintf(buf, sizeof(buf), "%ld°", lroundf(d.minC));
    tft_.setTextColor(kLow, bg);
    tft_.drawString(buf, cx, kDayLowY);
  }
}
