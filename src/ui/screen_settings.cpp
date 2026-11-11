#include "screen_settings.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kMargin = ui::lay::X(6);
constexpr int16_t kRowX = kMargin;
constexpr int16_t kRowW = config::kScreenWidth - 2 * kMargin;
constexpr int16_t kRowY0 = ui::lay::Y(40), kRowH = ui::lay::Y(30), kRowPitch = ui::lay::Y(33);
constexpr int16_t kTextX = kRowX + 12;
constexpr int16_t kChevronX = kRowX + kRowW - 12;
constexpr int16_t kValueRightX = kChevronX - 14;
constexpr int16_t kValueMaxW = ui::lay::X(130);
constexpr int16_t kTitleMaxW = ui::lay::X(120);
// "Photos" button in the header (top right)
constexpr int16_t kPhotosW = ui::lay::X(84), kPhotosH = ui::lay::Y(28);
constexpr int16_t kPhotosX = config::kScreenWidth - 8 - kPhotosW, kPhotosY = ui::lay::Y(4);
constexpr int16_t kPhotosHitX = kPhotosX - 10;
// "VI | EN" toggle left of it; the current language is highlighted
constexpr int16_t kLangW = ui::lay::X(60), kLangH = ui::lay::Y(28);
constexpr int16_t kLangX = kPhotosX - 8 - kLangW, kLangY = ui::lay::Y(4);
constexpr int16_t kLangHitX = kLangX - 10;
constexpr int16_t kLangRuleInset = ui::lay::X(8);

enum Row : int {
  kCitiesRow, kRotationRow, kBrightnessRow, kCalibrateRow, kWifiRow, kFirmwareRow, kRowCount
};

int16_t rowTop(int row) { return kRowY0 + row * kRowPitch; }

}  // namespace

void ScreenSettings::show(const UiState& state) {
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, tr(Str::Settings), kTitleMaxW);
  useFont(tft_, kFontSmall);
  ui::drawPill(tft_, kPhotosX, kPhotosY, kPhotosW, kPhotosH, tr(Str::Photos), kAccent, kAccent,
               true);
  drawLanguageToggle();
  drawCitiesRow(state);
  drawRotationRow(state);
  drawBrightnessRow(state);
  drawRow(kCalibrateRow, tr(Str::CalibrateTouch), "", kMuted);
  drawWifiRow(state);
  drawFirmwareRow(state);
}

void ScreenSettings::update(const UiState& state) {
  if (state.wifi != lastWifi_ || strcmp(state.ssid, lastSsid_) != 0) {
    drawWifiRow(state);
  }
  if (state.rotateSec != lastRotate_) {
    drawRotationRow(state);
  }
  // In Auto the row shows the level as it follows the room (cover the sensor to test).
  const bool autoMoved =
      state.brightness == Brightness::Auto && state.backlightPct != lastBacklightPct_;
  if (state.brightness != lastBrightness_ || autoMoved) {
    drawBrightnessRow(state);
  }
  if (state.citiesVersion != lastCities_) {
    drawCitiesRow(state);
  }
  if (state.ota != nullptr && state.ota->version != lastOta_) {
    drawFirmwareRow(state);
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

void ScreenSettings::drawBrightnessRow(const UiState& state) {
  lastBrightness_ = state.brightness;
  lastBacklightPct_ = state.backlightPct;
  char value[24];
  if (state.brightness == Brightness::Auto) {
    snprintf(value, sizeof(value), tr(Str::BrightnessAutoFmt), state.backlightPct);
  } else {
    const int level = static_cast<int>(state.brightness);
    strlcpy(value, tr(static_cast<Str>(static_cast<int>(Str::BrightnessAuto) + level)),
            sizeof(value));
  }
  drawRow(kBrightnessRow, tr(Str::Brightness), value, kHighlight);
}

void ScreenSettings::drawLanguageToggle() {
  useFont(tft_, kFontSmall);
  ui::drawPill(tft_, kLangX, kLangY, kLangW, kLangH, "", kCardBorder, kInk, true);
  const bool english = currentLanguage() == Language::English;
  const int16_t mid = kLangY + kLangH / 2;
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(english ? kMuted : kHighlight, kBackground);
  tft_.drawString(tr(Str::LanguageVi), kLangX + kLangW / 4 + 1, mid);
  tft_.setTextColor(english ? kHighlight : kMuted, kBackground);
  tft_.drawString(tr(Str::LanguageEn), kLangX + 3 * kLangW / 4 - 1, mid);
  tft_.drawFastVLine(kLangX + kLangW / 2, kLangY + kLangRuleInset, kLangH - 2 * kLangRuleInset,
                     kCardBorder);
}

void ScreenSettings::drawWifiRow(const UiState& state) {
  lastWifi_ = state.wifi;
  strlcpy(lastSsid_, state.ssid, sizeof(lastSsid_));
  const bool up = state.wifi == WifiState::Connected;
  drawRow(kWifiRow, tr(Str::Network), up ? state.ssid : tr(Str::NotConnected),
          up ? kOk : kMuted);
}

void ScreenSettings::drawFirmwareRow(const UiState& state) {
  char value[32];
  uint16_t color = kMuted;
  const OtaStatus* ota = state.ota;
  lastOta_ = ota != nullptr ? ota->version : 0;
  if (ota != nullptr && ota->state == OtaState::Available) {
    snprintf(value, sizeof(value), tr(Str::UpdateAvailableFmt), ota->latest);
    color = kHighlight;
  } else if (ota != nullptr && ota->state == OtaState::Downloading) {
    snprintf(value, sizeof(value), tr(Str::UpdateDownloadingFmt), ota->progressPct);
    color = kHighlight;
  } else {
    snprintf(value, sizeof(value), "v%s", config::kFirmwareVersion);
  }
  drawRow(kFirmwareRow, tr(Str::Firmware), value, color);
}

SettingsTarget ScreenSettings::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return SettingsTarget::Back;
  }
  if (ui::hit(x, y, kPhotosHitX, 0, config::kScreenWidth - kPhotosHitX, ui::kBackHitH)) {
    return SettingsTarget::Photos;
  }
  if (ui::hit(x, y, kLangHitX, 0, kPhotosHitX - kLangHitX, ui::kBackHitH)) {
    return SettingsTarget::Language;
  }
  // Rows plus half of the gap around them
  const int16_t gap = kRowPitch - kRowH;
  for (int row = 0; row < kRowCount; ++row) {
    if (ui::hit(x, y, kRowX, rowTop(row) - gap / 2, kRowW, kRowPitch)) {
      static const SettingsTarget kTargets[kRowCount] = {
          SettingsTarget::Cities, SettingsTarget::Rotation, SettingsTarget::Brightness,
          SettingsTarget::Calibrate, SettingsTarget::Wifi, SettingsTarget::Firmware};
      return kTargets[row];
    }
  }
  return SettingsTarget::None;
}
