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

constexpr int16_t kHeaderMidY = 18;
constexpr int16_t kBackChevronX = 14;
constexpr int16_t kTitleX = 26;
constexpr int16_t kBackHitW = 150, kBackHitH = 40;

constexpr int16_t kLangY = 44, kLangH = 48;
constexpr int16_t kSegW = 80, kSegH = 30, kSegGap = 4;
constexpr int16_t kSegEnX = kRowX + kRowW - 8 - kSegW;
constexpr int16_t kSegViX = kSegEnX - kSegGap - kSegW;
constexpr int16_t kSegY = kLangY + (kLangH - kSegH) / 2;

constexpr int16_t kCalY = kLangY + kLangH + 8, kCalH = 44;

constexpr int16_t kWifiY = kCalY + kCalH + 8, kWifiH = 60;
constexpr int16_t kWifiLine1 = kWifiY + 18, kWifiLine2 = kWifiY + 42;

}  // namespace

void ScreenSettings::show(const Palette& palette, const UiState& state) {
  palette_ = &palette;
  ui::paintSky(tft_, palette, 0, 0, config::kScreenWidth, config::kScreenHeight);

  // Header: "< Settings"
  const uint16_t sky = skyAt(palette, kHeaderMidY);
  ui::drawChevron(tft_, kBackChevronX, kHeaderMidY, false, kText, sky);
  useFont(tft_, kFontMedium);
  tft_.setTextColor(kText, sky);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::Settings), kTitleX, kHeaderMidY);

  drawLanguageRow();

  const uint16_t card = ui::drawCard(tft_, palette, kRowX, kCalY, kRowW, kCalH);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kText, card);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::CalibrateTouch), kTextX, kCalY + kCalH / 2);
  ui::drawChevron(tft_, kTextRightX, kCalY + kCalH / 2, true, kMuted, card);

  drawWifiRow(state);
}

void ScreenSettings::update(const UiState& state) {
  if (state.wifi != lastWifi_) {
    drawWifiRow(state);
  }
}

void ScreenSettings::drawLanguageRow() {
  const uint16_t card = ui::drawCard(tft_, *palette_, kRowX, kLangY, kRowW, kLangH);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kText, card);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::Language), kTextX, kLangY + kLangH / 2);

  // Segmented control; the active language is a solid white pill.
  const uint16_t track = blend(kTrackAlpha, kText, card);
  const int16_t trackX = kSegViX - 2, trackW = 2 * kSegW + kSegGap + 4;
  tft_.fillSmoothRoundRect(trackX, kSegY - 2, trackW, kSegH + 4, (kSegH + 4) / 2, track, card);

  const bool en = currentLanguage() == Language::English;
  const struct {
    int16_t x;
    const char* label;
    bool active;
  } segs[] = {{kSegViX, "Tiếng Việt", !en}, {kSegEnX, "English", en}};
  tft_.setTextDatum(MC_DATUM);
  for (const auto& seg : segs) {
    uint16_t bg = track;
    if (seg.active) {
      bg = kText;
      tft_.fillSmoothRoundRect(seg.x, kSegY, kSegW, kSegH, kSegH / 2, kText, track);
    }
    // The sky top is dark in every palette, so it reads well on the white pill.
    tft_.setTextColor(seg.active ? palette_->skyTop : kText, bg);
    tft_.drawString(seg.label, seg.x + kSegW / 2, kSegY + kSegH / 2);
  }
}

void ScreenSettings::drawWifiRow(const UiState& state) {
  lastWifi_ = state.wifi;
  const uint16_t card = ui::drawCard(tft_, *palette_, kRowX, kWifiY, kRowW, kWifiH);
  useFont(tft_, kFontSmall);

  tft_.setTextColor(kText, card);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(tr(Str::Network), kTextX, kWifiLine1);
  tft_.setTextDatum(MR_DATUM);
  tft_.drawString(state.ssid, kTextRightX, kWifiLine1);

  const bool up = state.wifi == WifiState::Connected;
  tft_.setTextColor(up ? kOk : kMuted, card);
  char line[40];
  if (up) {
    snprintf(line, sizeof(line), "%s  (%d dBm)", state.ip, state.rssi);
  } else {
    strlcpy(line, tr(Str::NotConnected), sizeof(line));
  }
  tft_.drawString(line, kTextRightX, kWifiLine2);
}

SettingsTarget ScreenSettings::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, 0, 0, kBackHitW, kBackHitH)) {
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
  return SettingsTarget::None;
}
