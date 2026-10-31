#include "screen_settings.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kMargin = 6;
constexpr int16_t kRowX = kMargin;
constexpr int16_t kRowW = config::kScreenWidth - 2 * kMargin;
constexpr int16_t kRowY0 = 40, kRowH = 34, kRowPitch = 39;
constexpr int16_t kTextX = kRowX + 12;
constexpr int16_t kChevronX = kRowX + kRowW - 12;
constexpr int16_t kValueRightX = kChevronX - 14;
constexpr int16_t kValueMaxW = 130;
constexpr int16_t kTitleMaxW = 280;

enum Row : int { kCitiesRow, kRotationRow, kLanguageRow, kCalibrateRow, kWifiRow, kRowCount };

int16_t rowTop(int row) { return kRowY0 + row * kRowPitch; }

}  // namespace

void ScreenSettings::show(const UiState& state) {
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, tr(Str::Settings), kTitleMaxW);
  drawCitiesRow(state);
  drawRotationRow(state);
  drawRow(kLanguageRow, tr(Str::Language), tr(Str::LanguageName), kMuted);
  drawRow(kCalibrateRow, tr(Str::CalibrateTouch), "", kMuted);
  drawWifiRow(state);
}

void ScreenSettings::update(const UiState& state) {
  if (state.wifi != lastWifi_ || strcmp(state.ssid, lastSsid_) != 0) {
    drawWifiRow(state);
  }
  if (state.rotateSec != lastRotate_) {
    drawRotationRow(state);
  }
  if (state.citiesVersion != lastCities_) {
    drawCitiesRow(state);
  }
}

void ScreenSettings::drawRow(int row, const char* label, const char* value,
                             uint16_t valueColor) {
  const int16_t top = rowTop(row);
  const int16_t mid = top + kRowH / 2;
  ui::drawCard(tft_, kRowX, top, kRowW, kRowH);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  tft_.drawString(label, kTextX, mid);
  tft_.setTextColor(valueColor, kBackground);
  tft_.setTextDatum(MR_DATUM);
  ui::drawFitted(tft_, value, kValueRightX, mid, kValueMaxW, kFontSmall, kFontTiny);
  ui::drawChevron(tft_, kChevronX, mid, true, kMuted, kBackground);
}

void ScreenSettings::drawCitiesRow(const UiState& state) {
  lastCities_ = state.citiesVersion;
  char value[12];
  snprintf(value, sizeof(value), "%u / %u", state.cities != nullptr ? state.cities->count : 0,
           config::kMaxCities);
  drawRow(kCitiesRow, tr(Str::Cities), value, kHighlight);
}

void ScreenSettings::drawRotationRow(const UiState& state) {
  lastRotate_ = state.rotateSec;
  char value[16];
  snprintf(value, sizeof(value), tr(Str::SecondsFmt), state.rotateSec);
  drawRow(kRotationRow, tr(Str::RotateEvery), value, kHighlight);
}

void ScreenSettings::drawWifiRow(const UiState& state) {
  lastWifi_ = state.wifi;
  strlcpy(lastSsid_, state.ssid, sizeof(lastSsid_));
  const bool up = state.wifi == WifiState::Connected;
  drawRow(kWifiRow, tr(Str::Network), up ? state.ssid : tr(Str::NotConnected),
          up ? kOk : kMuted);
}

SettingsTarget ScreenSettings::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return SettingsTarget::Back;
  }
  // Rows plus half of the gap around them
  const int16_t gap = kRowPitch - kRowH;
  for (int row = 0; row < kRowCount; ++row) {
    if (ui::hit(x, y, kRowX, rowTop(row) - gap / 2, kRowW, kRowPitch)) {
      static const SettingsTarget kTargets[kRowCount] = {
          SettingsTarget::Cities, SettingsTarget::Rotation, SettingsTarget::Language,
          SettingsTarget::Calibrate, SettingsTarget::Wifi};
      return kTargets[row];
    }
  }
  return SettingsTarget::None;
}
