#include "screen_main.h"

#include <math.h>

#include "assets/fonts.h"
#include "assets/icons.h"
#include "config.h"
#include "icon.h"
#include "theme.h"

using namespace theme;

namespace {

// Layout (landscape 320 x 240)
constexpr int16_t kMargin = 6;
constexpr int16_t kRadius = 8;

constexpr int16_t kHeaderMidY = 14;
constexpr int16_t kWifiDotR = 4;

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
  tft_.fillScreen(kBg);
  clock_.begin();
  firstFrame_ = true;
}

void ScreenMain::useFont(const uint8_t* font) {
  if (font != font_) {
    tft_.loadFont(font);  // unloads the previous font
    font_ = font;
  }
}

void ScreenMain::update(const UiState& s) {
  const bool newData = firstFrame_ || s.dataVersion != last_.dataVersion;
  const WeatherData* data = s.dataVersion > 0 ? s.data : nullptr;
  const struct tm* now = s.timeValid ? &s.localTime : nullptr;

  if (firstFrame_ || s.wifi != last_.wifi) {
    drawHeader(s.wifi);
  }
  if (newData) {
    drawWeather(data);
    drawDetails(data);
  }
  if (newData || s.fetch != last_.fetch) {
    drawFooter(s.fetch, data);
  }
  if (firstFrame_ || s.timeValid != last_.timeValid ||
      (s.timeValid && s.localTime.tm_mday != last_.localTime.tm_mday)) {
    drawDate(now);
  }
  clock_.update(now);

  last_ = s;
  firstFrame_ = false;
}

void ScreenMain::drawHeader(WifiState wifi) {
  tft_.fillRect(0, 0, config::kScreenWidth, kWeatherY - 2, kBg);

  useFont(kFontMedium);
  tft_.setTextColor(kText, kBg);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(config::kLocationName, kMargin + 2, kHeaderMidY);

  const char* label = "WiFi";
  uint16_t color = kOk;
  switch (wifi) {
    case WifiState::Connected:    break;
    case WifiState::Connecting:   label = "Đang kết nối"; color = kWarn;  break;
    case WifiState::WaitingRetry: label = "Mất WiFi";     color = kError; break;
    case WifiState::Idle:         label = "WiFi tắt";     color = kMuted; break;
  }
  const int16_t dotX = config::kScreenWidth - kMargin - kWifiDotR;
  tft_.fillSmoothCircle(dotX, kHeaderMidY, kWifiDotR, color, kBg);
  useFont(kFontSmall);
  tft_.setTextColor(color, kBg);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(label, dotX - kWifiDotR - 6, kHeaderMidY);
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
  tft_.fillRect(0, kWeatherY, kMargin + kLeftW + 2, kWeatherH, kBg);
  tft_.fillSmoothRoundRect(kMargin, kWeatherY, kLeftW, kWeatherH, kRadius, kCard, kBg);

  if (d == nullptr) {
    useFont(kFontSmall);
    tft_.setTextColor(kMuted, kCard);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString("Đang chờ dữ liệu...", kMargin + kLeftW / 2, kWeatherY + kWeatherH / 2);
    return;
  }

  drawIcon(tft_, weatherIcon(d->category, d->isDay), kIconX, kIconY, kCard);

  char buf[16];
  snprintf(buf, sizeof(buf), "%ld°", lroundf(d->temperatureC));
  useFont(kFontTemp);
  if (tft_.textWidth(buf) > kTempMaxW) {
    useFont(kFontMedium);
  }
  tft_.setTextColor(kText, kCard);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(buf, kTempRightX, kTempMidY);

  tft_.setTextColor(kAccent, kCard);
  tft_.setTextDatum(ML_DATUM);
  drawFitted(d->description, kDescX, kDescMidY, kDescMaxW);
}

void ScreenMain::drawDetails(const WeatherData* d) {
  tft_.fillRect(0, kDetailsY, kMargin + kLeftW + 2, kDetailsH, kBg);
  tft_.fillSmoothRoundRect(kMargin, kDetailsY, kLeftW, kDetailsH, kRadius, kCard, kBg);

  char values[kRows][16];
  if (d != nullptr) {
    snprintf(values[0], sizeof(values[0]), "%ld°", lroundf(d->apparentC));
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
      tft_.drawFastHLine(kRowIconX, top, kLeftW - 16, kCardLine);
    }
    drawIcon(tft_, *icons[i], kRowIconX, mid - kRowIconSize / 2, kCard);
    tft_.setTextColor(kMuted, kCard);
    tft_.setTextDatum(ML_DATUM);
    tft_.drawString(labels[i], kRowLabelX, mid);
    tft_.setTextColor(kText, kCard);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(values[i], kRowValueX, mid);
  }
}

void ScreenMain::drawFooter(FetchStatus fetch, const WeatherData* d) {
  tft_.fillRect(0, kFooterY, config::kScreenWidth, config::kScreenHeight - kFooterY, kBg);
  useFont(kFontSmall);

  char buf[32];
  snprintf(buf, sizeof(buf), "Cập nhật %s", d != nullptr ? d->observedAt : "--:--");
  tft_.setTextColor(kMuted, kBg);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(buf, kMargin + 2, kFooterMidY);

  const char* msg = "Chạm để làm mới";
  uint16_t color = kMuted;
  switch (fetch) {
    case FetchStatus::Fetching: msg = "Đang tải...";   color = kWarn;  break;
    case FetchStatus::Error:    msg = "Lỗi cập nhật";  color = kError; break;
    default: break;
  }
  tft_.setTextColor(color, kBg);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(msg, config::kScreenWidth - kMargin - 2, kFooterMidY);
}

void ScreenMain::drawDate(const struct tm* t) {
  tft_.fillRect(kClockX - 4, kDateY, kClockSize + 8, kDateH, kBg);
  useFont(kFontSmall);
  tft_.setTextColor(kMuted, kBg);
  tft_.setTextDatum(MC_DATUM);
  char buf[32];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%s, %02d/%02d", kWeekdays[t->tm_wday], t->tm_mday, t->tm_mon + 1);
  } else {
    strlcpy(buf, "Đang đồng bộ giờ...", sizeof(buf));
  }
  tft_.drawString(buf, kClockMidX, kDateY + kDateH / 2);
}
