#include "screen_settings.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kMargin = 8;
constexpr int16_t kRowX = kMargin;
constexpr int16_t kRowW = 320 - 2 * kMargin;
constexpr int16_t kTextX = kRowX + 12;
constexpr int16_t kTextRightX = kRowX + kRowW - 12;

constexpr int16_t kTitleMaxW = 280;
constexpr int16_t kChevronSpace = 14;  // right-aligned texts stop before the chevron

constexpr int16_t kLangY = 44, kLangH = 48;
constexpr int16_t kSegW = 80, kSegH = 30, kSegGap = 4;
constexpr int16_t kSegEnX = kRowX + kRowW - 8 - kSegW;
constexpr int16_t kSegViX = kSegEnX - kSegGap - kSegW;
constexpr int16_t kSegY = kLangY + (kLangH - kSegH) / 2;

constexpr int16_t kCalY = kLangY + kLangH + 8, kCalH = 44;

constexpr int16_t kWifiY = kCalY + kCalH + 8, kWifiH = 60;
constexpr int16_t kWifiLine1 = kWifiY + 18, kWifiLine2 = kWifiY + 42;

}  // namespace

void ScreenSettings::show(const UiState& state) {
  tft_.fillScreen(kBackground);

  ui::drawBackHeader(tft_, tr(Str::Settings), kTitleMaxW);

  drawLanguageRow();

  ui::drawCard(tft_, kRowX, kCalY, kRowW, kCalH);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::CalibrateTouch), kTextX, kCalY + kCalH / 2);
  ui::drawChevron(tft_, kTextRightX, kCalY + kCalH / 2, true, kMuted, kBackground);

  drawWifiRow(state);
}

void ScreenSettings::update(const UiState& state) {
  if (state.wifi != lastWifi_ || strcmp(state.ssid, lastSsid_) != 0) {
    drawWifiRow(state);
  }
}

void ScreenSettings::drawLanguageRow() {
  ui::drawCard(tft_, kRowX, kLangY, kRowW, kLangH);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::Language), kTextX, kLangY + kLangH / 2);

  // Segmented control; the active language is a solid accent pill.
  const int16_t trackX = kSegViX - 2, trackW = 2 * kSegW + kSegGap + 4;
  tft_.fillSmoothRoundRect(trackX, kSegY - 2, trackW, kSegH + 4, (kSegH + 4) / 2, kTrack,
                           kBackground);

  const bool en = currentLanguage() == Language::English;
  const struct {
    int16_t x;
    const char* label;
    bool active;
  } segs[] = {{kSegViX, "Tiếng Việt", !en}, {kSegEnX, "English", en}};
  tft_.setTextDatum(MC_DATUM);
  for (const auto& seg : segs) {
    uint16_t bg = kTrack;
    if (seg.active) {
      bg = kAccent;
      tft_.fillSmoothRoundRect(seg.x, kSegY, kSegW, kSegH, kSegH / 2, kAccent, kTrack);
    }
    tft_.setTextColor(seg.active ? kOnAccent : kInk, bg);
    tft_.drawString(seg.label, seg.x + kSegW / 2, kSegY + kSegH / 2);
  }
}

void ScreenSettings::drawWifiRow(const UiState& state) {
  lastWifi_ = state.wifi;
  strlcpy(lastSsid_, state.ssid, sizeof(lastSsid_));
  ui::drawCard(tft_, kRowX, kWifiY, kRowW, kWifiH);
  useFont(tft_, kFontSmall);

  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::Network), kTextX, kWifiLine1);
  ui::drawChevron(tft_, kTextRightX, kWifiY + kWifiH / 2, true, kMuted, kBackground);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(state.ssid, kTextRightX - kChevronSpace, kWifiLine1);

  const bool up = state.wifi == WifiState::Connected;
  tft_.setTextColor(up ? kOk : kMuted, kBackground);
  char line[40];
  if (up) {
    snprintf(line, sizeof(line), "%s  (%d dBm)", state.ip, state.rssi);
  } else {
    strlcpy(line, tr(Str::NotConnected), sizeof(line));
  }
  tft_.drawString(line, kTextRightX - kChevronSpace, kWifiLine2);
}

SettingsTarget ScreenSettings::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return SettingsTarget::Back;
  }
  if (ui::hit(x, y, kSegViX, kLangY, kSegW + kSegGap / 2, kLangH)) {
    return SettingsTarget::Vietnamese;
  }
  if (ui::hit(x, y, kSegEnX - kSegGap / 2, kLangY, kSegW + kSegGap / 2 + kMargin, kLangH)) {
    return SettingsTarget::English;
  }
  if (ui::hit(x, y, kRowX, kCalY, kRowW, kCalH)) {
    return SettingsTarget::Calibrate;
  }
  if (ui::hit(x, y, kRowX, kWifiY, kRowW, kWifiH)) {
    return SettingsTarget::Wifi;
  }
  return SettingsTarget::None;
}
