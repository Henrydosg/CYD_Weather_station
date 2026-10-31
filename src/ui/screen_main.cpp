#include "screen_main.h"

#include "config.h"

namespace {

// Colors
constexpr uint16_t kBg = TFT_BLACK;
constexpr uint16_t kHeaderBg = 0x10A2;  // dark slate
constexpr uint16_t kText = TFT_WHITE;
constexpr uint16_t kMuted = TFT_LIGHTGREY;
constexpr uint16_t kAccent = TFT_CYAN;
constexpr uint16_t kCloud = 0xC618;
constexpr uint16_t kDarkCloud = 0x7BEF;
constexpr uint16_t kRain = 0x3D7F;

// Built-in TFT_eSPI fonts
constexpr uint8_t kFontSmall = 2;  // 16 px
constexpr uint8_t kFontMedium = 4; // 26 px
constexpr uint8_t kFontBig = 7;    // 48 px, 7-segment digits only

// Layout (landscape 320 x 240)
constexpr int16_t kHeaderH = 26;
constexpr int16_t kBodyY = kHeaderH;
constexpr int16_t kBodyH = 128;
constexpr int16_t kDetailsY = kBodyY + kBodyH;
constexpr int16_t kDetailsH = 58;
constexpr int16_t kFooterY = kDetailsY + kDetailsH;
constexpr int16_t kFooterH = config::kScreenHeight - kFooterY;
constexpr int16_t kMargin = 8;
constexpr int16_t kIconCx = 62;
constexpr int16_t kIconCy = kBodyY + 60;
constexpr int16_t kTempX = 128;
constexpr int16_t kTempY = kBodyY + 18;
constexpr int16_t kDescY = kBodyY + 82;
constexpr int16_t kColRightX = 168;

uint16_t wifiColor(WifiState s) {
  switch (s) {
    case WifiState::Connected:    return TFT_GREEN;
    case WifiState::Connecting:   return TFT_YELLOW;
    case WifiState::WaitingRetry: return TFT_RED;
    default:                      return kMuted;
  }
}

}  // namespace

void ScreenMain::begin() {
  tft_.setRotation(config::kScreenRotation);
  tft_.fillScreen(kBg);
  firstFrame_ = true;
}

void ScreenMain::update(const UiState& s) {
  const bool newData = firstFrame_ || s.dataVersion != last_.dataVersion;
  const WeatherData* data = s.dataVersion > 0 ? s.data : nullptr;

  if (firstFrame_ || s.wifi != last_.wifi) {
    drawHeader(s.wifi);
  }
  if (newData) {
    drawWeather(data);
  }
  if (newData || s.fetch != last_.fetch) {
    drawFooter(s.fetch, data);
  }

  last_ = s;
  firstFrame_ = false;
}

void ScreenMain::drawHeader(WifiState wifi) {
  tft_.fillRect(0, 0, config::kScreenWidth, kHeaderH, kHeaderBg);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kText, kHeaderBg);
  tft_.drawString(config::kLocationName, kMargin, kHeaderH / 2, kFontSmall);

  const uint16_t color = wifiColor(wifi);
  tft_.setTextDatum(MR_DATUM);
  tft_.setTextColor(color, kHeaderBg);
  const int16_t textRight = config::kScreenWidth - kMargin - 14;
  tft_.drawString(wifi == WifiState::Connected ? "WiFi" : wifiStateName(wifi), textRight,
                  kHeaderH / 2, kFontSmall);
  tft_.fillCircle(config::kScreenWidth - kMargin - 4, kHeaderH / 2, 4, color);
}

void ScreenMain::drawWeather(const WeatherData* d) {
  tft_.fillRect(0, kBodyY, config::kScreenWidth, kBodyH + kDetailsH, kBg);

  if (d == nullptr) {
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(kMuted, kBg);
    tft_.drawString("Waiting for data...", config::kScreenWidth / 2, kBodyY + kBodyH / 2,
                    kFontMedium);
    return;
  }

  drawIcon(kIconCx, kIconCy, d->category, d->isDay);

  // Temperature: big 7-segment digits followed by a degree mark and unit.
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f", d->temperatureC);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(kText, kBg);
  const int16_t w = tft_.drawString(buf, kTempX, kTempY, kFontBig);
  const int16_t degX = kTempX + w + 8;
  tft_.drawCircle(degX, kTempY + 5, 4, kText);
  tft_.drawCircle(degX, kTempY + 5, 3, kText);
  tft_.drawString("C", degX + 7, kTempY, kFontMedium);

  // Provider descriptions can be long ("thunderstorm with heavy drizzle").
  const int16_t descMaxW = config::kScreenWidth - kTempX - kMargin;
  const uint8_t descFont =
      tft_.textWidth(d->description, kFontMedium) <= descMaxW ? kFontMedium : kFontSmall;
  tft_.setTextColor(kAccent, kBg);
  tft_.drawString(d->description, kTempX, kDescY, descFont);

  // Details: two columns, two rows.
  tft_.drawFastHLine(kMargin, kDetailsY, config::kScreenWidth - 2 * kMargin, kHeaderBg);
  const int16_t row1 = kDetailsY + 8;
  const int16_t row2 = row1 + 24;
  tft_.setTextColor(kMuted, kBg);

  snprintf(buf, sizeof(buf), "Feels %.1f C", d->apparentC);
  tft_.drawString(buf, kMargin, row1, kFontSmall);
  snprintf(buf, sizeof(buf), "Humidity %.0f %%", d->humidityPct);
  tft_.drawString(buf, kColRightX, row1, kFontSmall);
  snprintf(buf, sizeof(buf), "Wind %.1f km/h", d->windKmh);
  tft_.drawString(buf, kMargin, row2, kFontSmall);
  snprintf(buf, sizeof(buf), "Pressure %.0f hPa", d->pressureHpa);
  tft_.drawString(buf, kColRightX, row2, kFontSmall);
}

void ScreenMain::drawFooter(FetchStatus fetch, const WeatherData* d) {
  tft_.fillRect(0, kFooterY, config::kScreenWidth, kFooterH, kHeaderBg);
  const int16_t y = kFooterY + kFooterH / 2;

  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kMuted, kHeaderBg);
  char buf[24];
  snprintf(buf, sizeof(buf), "Updated %s", d != nullptr ? d->observedAt : "--:--");
  tft_.drawString(buf, kMargin, y, kFontSmall);

  const char* msg = "Tap to refresh";
  uint16_t color = kMuted;
  switch (fetch) {
    case FetchStatus::Fetching: msg = "Updating...";   color = TFT_YELLOW; break;
    case FetchStatus::Error:    msg = "Update failed"; color = TFT_RED;    break;
    default: break;
  }
  tft_.setTextDatum(MR_DATUM);
  tft_.setTextColor(color, kHeaderBg);
  tft_.drawString(msg, config::kScreenWidth - kMargin, y, kFontSmall);
}

// ---------------------------------------------------------------------------
// Icons, drawn with primitives (~100 x 100 px box centred on cx, cy)
// ---------------------------------------------------------------------------

void ScreenMain::drawSun(int16_t cx, int16_t cy, int16_t r) {
  constexpr int kRays = 8;
  for (int i = 0; i < kRays; ++i) {
    const float a = i * TWO_PI / kRays;
    tft_.drawWideLine(cx + cosf(a) * (r + 5), cy + sinf(a) * (r + 5), cx + cosf(a) * (r + 13),
                      cy + sinf(a) * (r + 13), 3, TFT_YELLOW, kBg);
  }
  tft_.fillCircle(cx, cy, r, TFT_YELLOW);
}

void ScreenMain::drawMoon(int16_t cx, int16_t cy, int16_t r) {
  tft_.fillCircle(cx, cy, r, 0xFFF0);
  tft_.fillCircle(cx + r / 2, cy - r / 3, r - 2, kBg);
}

void ScreenMain::drawCloud(int16_t cx, int16_t cy, uint16_t color) {
  tft_.fillCircle(cx - 18, cy + 4, 16, color);
  tft_.fillCircle(cx + 2, cy - 8, 22, color);
  tft_.fillCircle(cx + 24, cy + 4, 16, color);
  tft_.fillRoundRect(cx - 34, cy + 4, 74, 18, 9, color);
}

void ScreenMain::drawIcon(int16_t cx, int16_t cy, WeatherCategory category, bool isDay) {
  auto drawSky = [&](int16_t x, int16_t y, int16_t r) {
    if (isDay) {
      drawSun(x, y, r);
    } else {
      drawMoon(x, y, r);
    }
  };

  switch (category) {
    case WeatherCategory::Clear:
      drawSky(cx, cy, 24);
      break;

    case WeatherCategory::PartlyCloudy:
      drawSky(cx - 14, cy - 16, 16);
      drawCloud(cx + 6, cy + 10, kCloud);
      break;

    case WeatherCategory::Cloudy:
      drawCloud(cx, cy, kCloud);
      break;

    case WeatherCategory::Fog:
      for (int i = 0; i < 4; ++i) {
        tft_.fillRoundRect(cx - 36 + (i % 2) * 8, cy - 22 + i * 14, 64, 6, 3, kCloud);
      }
      break;

    case WeatherCategory::Drizzle:
    case WeatherCategory::Rain: {
      drawCloud(cx, cy - 12, kDarkCloud);
      const int drops = category == WeatherCategory::Rain ? 4 : 3;
      for (int i = 0; i < drops; ++i) {
        const int16_t x = cx - 24 + i * 16;
        tft_.drawWideLine(x, cy + 20, x - 6, cy + 36, 3, kRain, kBg);
      }
      break;
    }

    case WeatherCategory::Snow:
      drawCloud(cx, cy - 12, kCloud);
      for (int i = 0; i < 4; ++i) {
        tft_.fillCircle(cx - 24 + i * 16, cy + 26 + (i % 2) * 8, 4, TFT_WHITE);
      }
      break;

    case WeatherCategory::Thunderstorm:
      drawCloud(cx, cy - 12, kDarkCloud);
      tft_.fillTriangle(cx + 2, cy + 14, cx - 12, cy + 32, cx, cy + 32, TFT_YELLOW);
      tft_.fillTriangle(cx - 4, cy + 28, cx + 10, cy + 28, cx - 6, cy + 46, TFT_YELLOW);
      break;

    case WeatherCategory::Unknown:
      tft_.setTextDatum(MC_DATUM);
      tft_.setTextColor(kMuted, kBg);
      tft_.drawString("?", cx, cy, kFontMedium);
      break;
  }
}
