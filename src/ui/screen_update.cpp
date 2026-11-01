#include "screen_update.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kH = config::kScreenHeight;
constexpr int16_t kTitleMaxW = 280;
constexpr int16_t kBodyY = 44;
constexpr int16_t kVersionMidY = 70;
constexpr int16_t kStatusMidY = 102;
constexpr int16_t kBarX = 30, kBarY = 122, kBarW = kW - 60, kBarH = 12;
constexpr int16_t kNoteMidY = 152;
constexpr int16_t kPctMidY = 170, kPctBoxW = 140, kPctBoxH = 16;  // "Downloading 42%"
constexpr int16_t kButtonW = 180, kButtonH = 40;
constexpr int16_t kButtonX = (kW - kButtonW) / 2, kButtonY = 182;
constexpr int16_t kTextMaxW = kW - 24;
constexpr uint8_t kPercent = 100;

bool online(const UiState& s) { return s.wifi == WifiState::Connected; }

}  // namespace

void ScreenUpdate::show(const UiState& s) {
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, tr(Str::UpdateTitle), kTitleMaxW);
  drawBody(s);
}

void ScreenUpdate::update(const UiState& s) {
  if (s.ota == nullptr || (s.ota->version == lastOta_ && online(s) == lastOnline_)) {
    return;
  }
  if (s.ota->state == OtaState::Downloading && lastState_ == OtaState::Downloading &&
      online(s) == lastOnline_) {
    lastOta_ = s.ota->version;
    drawProgress(s.ota->progressPct);  // no full redraw for every percent
    return;
  }
  drawBody(s);
}

bool ScreenUpdate::hasButton(const UiState& s) {
  if (s.ota == nullptr || !online(s)) {
    return false;
  }
  switch (s.ota->state) {
    case OtaState::Idle:
    case OtaState::UpToDate:
    case OtaState::Available:
    case OtaState::Failed:
      return true;
    default:
      return false;
  }
}

UpdateTarget ScreenUpdate::hitTest(int16_t x, int16_t y, const UiState& s) const {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return UpdateTarget::Back;
  }
  if (hasButton(s) && ui::hit(x, y, kButtonX, kButtonY, kButtonW, kButtonH)) {
    return UpdateTarget::Action;
  }
  return UpdateTarget::None;
}

void ScreenUpdate::drawBody(const UiState& s) {
  const OtaStatus empty;
  const OtaStatus& ota = s.ota != nullptr ? *s.ota : empty;
  lastOta_ = ota.version;
  lastState_ = ota.state;
  lastOnline_ = online(s);
  tft_.fillRect(0, kBodyY, kW, kH - kBodyY, kBackground);

  char buf[64];
  tft_.setTextDatum(MC_DATUM);
  snprintf(buf, sizeof(buf), tr(Str::UpdateVersionFmt), config::kFirmwareVersion);
  tft_.setTextColor(kInk, kBackground);
  ui::drawFitted(tft_, buf, kW / 2, kVersionMidY, kTextMaxW, kFontMedium, kFontSmall);

  uint16_t color = kMuted;
  const char* note = nullptr;
  if (!online(s) && ota.state != OtaState::Downloading && ota.state != OtaState::Restarting) {
    strlcpy(buf, tr(Str::UpdateNoWifi), sizeof(buf));
    color = kError;
  } else {
    switch (ota.state) {
      case OtaState::Idle:
        strlcpy(buf, tr(Str::UpdateTapCheck), sizeof(buf));
        break;
      case OtaState::Checking:
        strlcpy(buf, tr(Str::UpdateChecking), sizeof(buf));
        break;
      case OtaState::UpToDate:
        strlcpy(buf, tr(Str::UpdateUpToDate), sizeof(buf));
        color = kOk;
        break;
      case OtaState::Available:
        snprintf(buf, sizeof(buf), tr(Str::UpdateAvailableFmt), ota.latest);
        color = kHighlight;
        note = tr(Str::UpdateKeepPower);
        break;
      case OtaState::Downloading:
        snprintf(buf, sizeof(buf), tr(Str::UpdateAvailableFmt), ota.latest);
        color = kHighlight;
        note = tr(Str::UpdateKeepPower);
        break;
      case OtaState::Failed:
        strlcpy(buf, tr(Str::UpdateFailed), sizeof(buf));
        color = kError;
        break;
      case OtaState::Restarting:
        strlcpy(buf, tr(Str::UpdateRestarting), sizeof(buf));
        color = kOk;
        break;
    }
  }
  tft_.setTextColor(color, kBackground);
  ui::drawFitted(tft_, buf, kW / 2, kStatusMidY, kTextMaxW, kFontSmall, kFontTiny);

  if (ota.state == OtaState::Downloading) {
    drawProgress(ota.progressPct);
  }
  if (note != nullptr) {
    useFont(tft_, kFontTiny);
    tft_.setTextColor(kMuted, kBackground);
    tft_.drawString(note, kW / 2, kNoteMidY);
  }
  if (hasButton(s)) {
    useFont(tft_, kFontSmall);
    const bool install = ota.state == OtaState::Available;
    ui::drawPill(tft_, kButtonX, kButtonY, kButtonW, kButtonH,
                 tr(install ? Str::UpdateInstall : Str::UpdateCheck), kAccent, kOnAccent, false);
  }
}

void ScreenUpdate::drawProgress(uint8_t pct) {
  const int16_t filled = static_cast<int16_t>(static_cast<int32_t>(kBarW) * pct / kPercent);
  tft_.fillRect(kBarX, kBarY, filled, kBarH, kAccent);
  tft_.fillRect(kBarX + filled, kBarY, kBarW - filled, kBarH, kTrack);
  char buf[16];
  snprintf(buf, sizeof(buf), tr(Str::UpdateDownloadingFmt), pct);
  tft_.fillRect(kW / 2 - kPctBoxW / 2, kPctMidY - kPctBoxH / 2, kPctBoxW, kPctBoxH,
                kBackground);  // the previous value
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(buf, kW / 2, kPctMidY);
}
