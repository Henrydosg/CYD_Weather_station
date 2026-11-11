#include "screen_info.h"

#include <string.h>

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kTitleMaxW = ui::lay::X(280);
constexpr int16_t kCardX = ui::lay::X(6);
constexpr int16_t kCardY = ui::lay::Y(38);
constexpr int16_t kCardW = kW - 12;
constexpr int16_t kCardH = ui::lay::Y(196);
constexpr int16_t kLabelX = ui::lay::X(16), kValueRightX = kW - 16;
constexpr int16_t kRowMid0 = ui::lay::Y(50), kRowPitch = ui::lay::Y(21);
constexpr int16_t kValueLeftX = ui::lay::X(100);  // values never reach further left
constexpr int16_t kValueMaxW = kValueRightX - kValueLeftX;
constexpr int16_t kClearHalfH = ui::lay::Y(9);    // value area above / below the row centre
constexpr int16_t kRuleOffset = ui::lay::Y(10);   // divider below the row centre
constexpr uint32_t kSecPerMin = 60, kSecPerHour = 3600, kSecPerDay = 86400;
constexpr uint32_t kBytesPerKb = 1024;

enum Row : int { kVersion, kUptime, kBoots, kLastReset, kWifi, kIp, kMac, kRam, kWeather };

constexpr Str kLabels[ScreenInfo::kRows] = {Str::InfoVersion, Str::InfoUptime,
                                            Str::InfoBoots,   Str::InfoLastReset,
                                            Str::InfoWifi,    Str::InfoIp,
                                            Str::InfoMac,     Str::InfoRam,
                                            Str::InfoWeather};

int16_t rowMid(int row) { return kRowMid0 + row * kRowPitch; }

// Text and colour of one value row.
uint16_t valueText(int row, const UiState& s, char* buf, size_t len) {
  const SystemInfo& sys = s.system;
  const BootReport* boot = sys.boot;
  const bool online = s.wifi == WifiState::Connected;
  switch (row) {
    case kVersion: {
      if (boot != nullptr && boot->verifying) {
        const uint32_t windowSec = config::kOtaVerifyWindowMs / 1000;
        const uint32_t left = sys.uptimeSec < windowSec ? windowSec - sys.uptimeSec : 0;
        snprintf(buf, len, tr(Str::InfoVerifyingFmt), config::kFirmwareVersion,
                 static_cast<unsigned>((left + kSecPerMin - 1) / kSecPerMin));
        return kHighlight;
      }
      snprintf(buf, len, tr(Str::InfoConfirmedFmt), config::kFirmwareVersion);
      return kOk;
    }
    case kUptime: {
      const uint32_t up = sys.uptimeSec;
      const unsigned h = (up % kSecPerDay) / kSecPerHour;
      const unsigned m = (up % kSecPerHour) / kSecPerMin;
      if (up >= kSecPerDay) {
        snprintf(buf, len, tr(Str::InfoUptimeDaysFmt),
                 static_cast<unsigned long>(up / kSecPerDay), h, m);
      } else {
        snprintf(buf, len, "%02u:%02u:%02u", h, m, static_cast<unsigned>(up % kSecPerMin));
      }
      return kInk;
    }
    case kBoots:
      snprintf(buf, len, tr(Str::InfoBootsFmt),
               static_cast<unsigned long>(boot != nullptr ? boot->boots : 0),
               static_cast<unsigned long>(boot != nullptr ? boot->crashes : 0));
      return boot != nullptr && boot->crashes > 0 ? kWarn : kInk;
    case kLastReset: {
      const int kind = static_cast<int>(boot != nullptr ? boot->reset : ResetKind::Other);
      strlcpy(buf, tr(static_cast<Str>(static_cast<int>(Str::ResetPowerOn) + kind)), len);
      return kInk;
    }
    case kWifi:
      if (!online) {
        strlcpy(buf, tr(Str::NotConnected), len);
        return kMuted;
      }
      snprintf(buf, len, "%s   %d dBm", s.ssid, s.rssi);
      return kOk;
    case kIp:
      strlcpy(buf, online ? s.ip : "-", len);
      return kInk;
    case kMac:
      strlcpy(buf, sys.mac != nullptr ? sys.mac : "", len);
      return kInk;
    case kRam:
      snprintf(buf, len, tr(Str::InfoRamFmt),
               static_cast<unsigned long>(sys.freeHeap / kBytesPerKb),
               static_cast<unsigned long>(sys.minFreeHeap / kBytesPerKb));
      return kInk;
    case kWeather:
      if (sys.weatherAgeSec < 0) {
        strlcpy(buf, tr(Str::InfoWeatherNone), len);
        return kMuted;
      }
      if (static_cast<uint32_t>(sys.weatherAgeSec) < kSecPerMin) {
        strlcpy(buf, tr(Str::InfoWeatherNow), len);
      } else {
        snprintf(buf, len, tr(Str::InfoWeatherAgoFmt),
                 static_cast<unsigned long>(sys.weatherAgeSec / kSecPerMin));
      }
      return kInk;
    default:
      buf[0] = '\0';
      return kInk;
  }
}

}  // namespace

void ScreenInfo::show(const UiState& state) {
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, tr(Str::InfoTitle), kTitleMaxW);
  ui::drawCard(tft_, kCardX, kCardY, kCardW, kCardH);
  useFont(tft_, kFontLabel);
  tft_.setTextDatum(ML_DATUM);
  tft_.setTextColor(kLabel, kBackground);
  for (int row = 0; row < kRows; ++row) {
    tft_.drawString(tr(kLabels[row]), kLabelX, rowMid(row));
    if (row < kRows - 1) {
      tft_.drawFastHLine(kLabelX, rowMid(row) + kRuleOffset, kValueRightX - kLabelX, kTrack);
    }
  }
  drawValues(state, true);
}

void ScreenInfo::update(const UiState& state) {
  if (state.system.uptimeSec != lastUptime_) {
    drawValues(state, false);
  }
}

bool ScreenInfo::hitBack(int16_t x, int16_t y) const {
  return ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH);
}

void ScreenInfo::drawValues(const UiState& state, bool force) {
  lastUptime_ = state.system.uptimeSec;
  char buf[sizeof(last_[0])];
  for (int row = 0; row < kRows; ++row) {
    const uint16_t color = valueText(row, state, buf, sizeof(buf));
    if (!force && color == lastColor_[row] && strcmp(buf, last_[row]) == 0) {
      continue;
    }
    strlcpy(last_[row], buf, sizeof(last_[row]));
    lastColor_[row] = color;
    const int16_t mid = rowMid(row);
    tft_.fillRect(kValueLeftX, mid - kClearHalfH, kValueMaxW, 2 * kClearHalfH, kBackground);
    tft_.setTextDatum(MR_DATUM);
    tft_.setTextColor(color, kBackground);
    ui::drawFitted(tft_, buf, kValueRightX, mid, kValueMaxW, kFontLabel, kFontTiny);
  }
}
