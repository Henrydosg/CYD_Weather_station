#include "screen_main.h"

#include <math.h>

#include "assets/fonts.h"
#include "assets/icons.h"
#include "config.h"
#include "icon.h"

using namespace theme;

namespace {

// Layout (landscape 320 x 240)
constexpr int16_t kMargin = 6;
constexpr int16_t kRadius = 10;

constexpr int16_t kHeaderH = 28;
constexpr int16_t kHeaderMidY = 14;

// WiFi signal bars (bottom-aligned, right edge of the header)
constexpr int kBarCount = 4;
constexpr int16_t kBarW = 3, kBarGap = 2, kBarMinH = 4, kBarStepH = 3, kBarBottomY = 20;
constexpr int16_t kBarsX = 320 - kMargin - kBarCount * (kBarW + kBarGap) + kBarGap;
constexpr int8_t kRssiLevels[kBarCount] = {-85, -75, -65, -55};  // dBm for 1..4 bars

constexpr int16_t kLeftW = 166;
constexpr int16_t kWeatherY = 30;
constexpr int16_t kWeatherH = 104;
constexpr int16_t kIconX = kMargin + 4;
constexpr int16_t kIconY = kWeatherY + 4;
constexpr int16_t kIconSize = 72;
constexpr int16_t kTempRightX = kMargin + kLeftW - 8;
constexpr int16_t kTempMidY = kIconY + kIconSize / 2;
constexpr int16_t kTempMaxW = kTempRightX - (kIconX + kIconSize + 2);
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
constexpr int16_t kHumBarX = 88, kHumBarW = 30, kHumBarH = 4;

constexpr int16_t kClockX = 178;
constexpr int16_t kClockY = 30;
constexpr int16_t kClockSize = 136;
constexpr int16_t kClockMidX = kClockX + kClockSize / 2;
constexpr int16_t kDigitalX = kClockMidX - 69;
constexpr int16_t kDigitalY = kClockY + kClockSize + 2;
constexpr int16_t kDateY = kDigitalY + 30;
constexpr int16_t kDateH = 18;

constexpr int16_t kFooterY = 222;
constexpr int16_t kFooterMidY = kFooterY + 9;

constexpr uint32_t kMinutesPerHour = 60;

const char* const kWeekdays[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư",
                                 "Thứ Năm",  "Thứ Sáu", "Thứ Bảy"};

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

// Remove the last UTF-8 character from `s` (in place).
void popUtf8Char(char* s) {
  size_t len = strlen(s);
  while (len > 0) {
    const unsigned char c = static_cast<unsigned char>(s[--len]);
    s[len] = '\0';
    if ((c & 0xC0) != 0x80) {  // reached the lead byte
      break;
    }
  }
}

}  // namespace

ScreenMain::ScreenMain(TFT_eSPI& tft)
    : tft_(tft), clock_(tft, kClockX, kClockY, kDigitalX, kDigitalY) {}

void ScreenMain::begin() {
  tft_.setRotation(config::kScreenRotation);
  tft_.invertDisplay(config::kInvertDisplay);
  clock_.begin();
  firstFrame_ = true;
}

void ScreenMain::useFont(const uint8_t* font) {
  if (font != font_) {
    tft_.loadFont(font);  // unloads the previous font
    font_ = font;
  }
}

void ScreenMain::paintSky(int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t row = y; row < y + h; ++row) {
    tft_.drawFastHLine(x, row, w, skyAt(*palette_, row));
  }
}

// Translucent card: paints the sky behind it, then a lighter rounded rect.
// Returns the card colour (use it as the text/icon background).
uint16_t ScreenMain::drawCard(int16_t x, int16_t y, int16_t w, int16_t h) {
  paintSky(x - 1, y - 1, w + 2, h + 2);
  const uint16_t sky = skyAt(*palette_, y + h / 2);
  const uint16_t card = blend(kCardAlpha, kText, sky);
  tft_.fillSmoothRoundRect(x, y, w, h, kRadius, card, sky);
  return card;
}

void ScreenMain::update(const UiState& s) {
  const WeatherData* data = s.dataVersion > 0 ? s.data : nullptr;
  const struct tm* now = s.timeValid ? &s.localTime : nullptr;

  // A new palette (weather or day/night changed) repaints the whole screen.
  const Palette& palette = data != nullptr ? paletteFor(data->category, data->isDay)
                                           : defaultPalette();
  const bool full = firstFrame_ || &palette != palette_;
  if (full) {
    palette_ = &palette;
    paintSky(0, 0, config::kScreenWidth, config::kScreenHeight);
    clock_.setPalette(palette);
  }

  const bool newData = full || s.dataVersion != last_.dataVersion;
  const uint8_t bars = signalBars(s.wifi, s.rssi);

  if (full || s.wifi != last_.wifi || bars != lastBars_) {
    drawHeader(s.wifi, bars);
    lastBars_ = bars;
  }
  if (newData) {
    drawWeather(data);
    drawDetails(data);
  }
  if (newData || s.fetch != last_.fetch || s.dataAgeMin != last_.dataAgeMin) {
    drawFooter(s.fetch, data, s.dataAgeMin);
  }
  if (full || s.timeValid != last_.timeValid ||
      (s.timeValid && s.localTime.tm_mday != last_.localTime.tm_mday)) {
    drawDate(now);
  }
  clock_.update(now);

  last_ = s;
  firstFrame_ = false;
}

void ScreenMain::drawHeader(WifiState wifi, uint8_t bars) {
  paintSky(0, 0, config::kScreenWidth, kHeaderH);

  useFont(kFontMedium);
  tft_.setTextColor(kText, skyAt(*palette_, kHeaderMidY));
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(config::kLocationName, kMargin + 2, kHeaderMidY);

  // Signal bars: green by strength; one yellow/red bar while (re)connecting.
  uint16_t on = kOk;
  if (wifi == WifiState::Connecting) {
    on = kWarn;
    bars = 1;
  } else if (wifi != WifiState::Connected) {
    on = kError;
    bars = 1;
  }
  const uint16_t sky = skyAt(*palette_, kBarBottomY);
  const uint16_t off = blend(kTrackAlpha, kText, sky);
  for (int i = 0; i < kBarCount; ++i) {
    const int16_t h = kBarMinH + i * kBarStepH;
    const int16_t x = kBarsX + i * (kBarW + kBarGap);
    tft_.fillRect(x, kBarBottomY - h, kBarW, h, i < bars ? on : off);
  }
}

void ScreenMain::drawFitted(const char* text, int16_t x, int16_t y, int16_t maxWidth) {
  useFont(kFontMedium);
  if (tft_.textWidth(text) > maxWidth) {
    useFont(kFontSmall);
  }
  char buf[sizeof(WeatherData::description) + 4];
  strlcpy(buf, text, sizeof(buf));
  if (tft_.textWidth(buf) > maxWidth) {
    // Still too long: trim characters and append "..".
    do {
      popUtf8Char(buf);
      char probe[sizeof(buf)];
      snprintf(probe, sizeof(probe), "%s..", buf);
      if (tft_.textWidth(probe) <= maxWidth || buf[0] == '\0') {
        strlcpy(buf, probe, sizeof(buf));
        break;
      }
    } while (true);
  }
  tft_.drawString(buf, x, y);
}

void ScreenMain::drawWeather(const WeatherData* d) {
  const uint16_t card = drawCard(kMargin, kWeatherY, kLeftW, kWeatherH);

  if (d == nullptr) {
    useFont(kFontSmall);
    tft_.setTextColor(kMuted, card);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("Đang chờ dữ liệu...", kMargin + kLeftW / 2, kWeatherY + kWeatherH / 2);
    return;
  }

  drawIcon(tft_, weatherIcon(d->category, d->isDay), kIconX, kIconY, card);

  char buf[16];
  snprintf(buf, sizeof(buf), "%ld°", lroundf(d->temperatureC));
  useFont(kFontTemp);
  if (tft_.textWidth(buf) > kTempMaxW) {
    useFont(kFontMedium);
  }
  tft_.setTextColor(temperatureColor(d->temperatureC), card);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(buf, kTempRightX, kTempMidY);

  tft_.setTextColor(palette_->accent, card);
  tft_.setTextDatum(ML_DATUM);
  drawFitted(d->description, kDescX, kDescMidY, kDescMaxW);
}

void ScreenMain::drawDetails(const WeatherData* d) {
  const uint16_t card = drawCard(kMargin, kDetailsY, kLeftW, kDetailsH);
  const uint16_t line = blend(kLineAlpha, kText, card);

  char values[kRows][16];
  uint16_t valueColor[kRows] = {kText, kText, kText, kText};
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
  const char* labels[kRows] = {"Cảm giác", "Độ ẩm", "Gió", "Áp suất"};

  useFont(kFontSmall);
  for (int i = 0; i < kRows; ++i) {
    const int16_t top = kDetailsY + 1 + i * kRowH;
    const int16_t mid = top + kRowH / 2;
    if (i > 0) {
      tft_.drawFastHLine(kRowIconX, top, kLeftW - 16, line);
    }
    drawIcon(tft_, *icons[i], kRowIconX, mid - kRowIconSize / 2, card);
    tft_.setTextColor(kMuted, card);
    tft_.setTextDatum(ML_DATUM);
    tft_.drawString(labels[i], kRowLabelX, mid);
    tft_.setTextColor(valueColor[i], card);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(values[i], kRowValueX, mid);
  }

  // Humidity bar between the label and the value.
  const int16_t barY = kDetailsY + 1 + kHumidityRow * kRowH + (kRowH - kHumBarH) / 2;
  tft_.fillSmoothRoundRect(kHumBarX, barY, kHumBarW, kHumBarH, kHumBarH / 2,
                           blend(kTrackAlpha, kText, card), card);
  if (d != nullptr) {
    const float pct = constrain(d->humidityPct, 0.0f, 100.0f);
    const int16_t w = max<int16_t>(kHumBarH, lroundf(kHumBarW * pct / 100.0f));
    tft_.fillSmoothRoundRect(kHumBarX, barY, w, kHumBarH, kHumBarH / 2, kBarFill, card);
  }
}

void ScreenMain::drawFooter(FetchStatus fetch, const WeatherData* d, uint32_t ageMin) {
  paintSky(0, kFooterY, config::kScreenWidth, config::kScreenHeight - kFooterY);
  const uint16_t sky = skyAt(*palette_, kFooterMidY);
  useFont(kFontSmall);

  char buf[32];
  if (d == nullptr) {
    strlcpy(buf, "Chưa có dữ liệu", sizeof(buf));
  } else if (ageMin == 0) {
    strlcpy(buf, "Vừa cập nhật", sizeof(buf));
  } else if (ageMin < kMinutesPerHour) {
    snprintf(buf, sizeof(buf), "%lu phút trước", static_cast<unsigned long>(ageMin));
  } else {
    snprintf(buf, sizeof(buf), "%lu giờ trước", static_cast<unsigned long>(ageMin / kMinutesPerHour));
  }
  tft_.setTextColor(kMuted, sky);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(buf, kMargin + 2, kFooterMidY);

  const char* msg = "Chạm để làm mới";
  uint16_t color = kMuted;
  switch (fetch) {
    case FetchStatus::Fetching: msg = "Đang tải...";   color = kWarn;  break;
    case FetchStatus::Error:    msg = "Lỗi cập nhật";  color = kError; break;
    default: break;
  }
  tft_.setTextColor(color, sky);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(msg, config::kScreenWidth - kMargin - 2, kFooterMidY);
}

void ScreenMain::drawDate(const struct tm* t) {
  paintSky(kClockX - 4, kDateY, kClockSize + 8, kDateH);
  useFont(kFontSmall);
  tft_.setTextColor(kMuted, skyAt(*palette_, kDateY + kDateH / 2));
  tft_.setTextDatum(MC_DATUM);
  char buf[32];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%s, %02d/%02d", kWeekdays[t->tm_wday], t->tm_mday, t->tm_mon + 1);
  } else {
    strlcpy(buf, "Đang đồng bộ giờ...", sizeof(buf));
  }
  tft_.drawString(buf, kClockMidX, kDateY + kDateH / 2);
}
