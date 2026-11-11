#include "screen_wifi.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;

// Header: "< WiFi" and an outlined "Rescan" pill
constexpr int16_t kTitleMaxW = ui::lay::X(180);
constexpr int16_t kRescanW = ui::lay::X(92), kRescanH = ui::lay::Y(28);
constexpr int16_t kRescanX = kW - 8 - kRescanW, kRescanY = ui::lay::Y(4);
constexpr int16_t kRescanHitX = kRescanX - 10;

constexpr int16_t kStatusX = ui::lay::X(10);
constexpr int16_t kStatusMidY = ui::lay::Y(46);
constexpr int16_t kStatusH = ui::lay::Y(16);
constexpr int16_t kStatusMaxW = kW - 2 * kStatusX;

// List card: four rows per page
constexpr int16_t kListX = ui::lay::X(6), kListY = ui::lay::Y(60), kListW = kW - 12;
constexpr int kRowsPerPage = 4;
constexpr int16_t kRowH = ui::lay::Y(36);
constexpr int16_t kListH = kRowsPerPage * kRowH;
constexpr int16_t kBarsX = ui::lay::X(18), kBarsBottomOffset = ui::lay::Y(7);
constexpr int16_t kSsidX = ui::lay::X(46), kSsidMaxW = ui::lay::X(208);
constexpr int16_t kLockX = ui::lay::X(262);
constexpr int16_t kMarkX = ui::lay::X(290);    // check mark / chevron centre
constexpr int16_t kSavedRightX = kListX + kListW - 12;
constexpr int8_t kRssiLevels[] = {-85, -75, -65, -55};  // dBm for 1..4 bars

// Pager
constexpr int16_t kPagerY = ui::lay::Y(210), kPagerH = ui::lay::Y(28);
constexpr int16_t kPagerBtnW = ui::lay::X(100);
constexpr int16_t kPrevX = ui::lay::X(6), kNextX = kW - 6 - kPagerBtnW;
constexpr int16_t kPagerHitPad = ui::lay::S(4);

uint8_t barsFor(int8_t rssi) {
  uint8_t bars = 1;
  for (uint8_t i = 1; i < sizeof(kRssiLevels); ++i) {
    if (rssi >= kRssiLevels[i]) {
      bars = i + 1;
    }
  }
  return bars;
}

void drawBars(TFT_eSPI& tft, int16_t x, int16_t bottom, uint8_t lit) {
  for (int i = 0; i < 4; ++i) {
    const int16_t h = 4 + i * 3;
    tft.fillRect(x + i * 5, bottom - h, 3, h, i < lit ? kAccent : kBarOff);
  }
}

void drawLock(TFT_eSPI& tft, int16_t x, int16_t midY, uint16_t color) {
  tft.fillSmoothRoundRect(x, midY - 2, 11, 8, 2, color, kBackground);
  // Shackle: upper half ring (TFT_eSPI arcs: 0 deg = 6 o'clock, clockwise).
  tft.drawSmoothArc(x + 5, midY - 2, 4, 2, 90, 270, color, kBackground, true);
}

void drawCheck(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawWedgeLine(cx - 5, cy, cx - 1, cy + 4, 1.2f, 1.2f, color, kBackground);
  tft.drawWedgeLine(cx - 1, cy + 4, cx + 6, cy - 4, 1.2f, 1.2f, color, kBackground);
}

bool isConnectedTo(const UiState& s, const char* ssid) {
  return s.wifi == WifiState::Connected && strcmp(s.ssid, ssid) == 0;
}

}  // namespace

void ScreenWifi::show(const UiState& state, bool resetPage) {
  if (resetPage) {
    page_ = 0;
  }
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, tr(Str::Network), kTitleMaxW);

  tft_.fillSmoothRoundRect(kRescanX, kRescanY, kRescanW, kRescanH, kRescanH / 2, kAccent,
                           kBackground);
  tft_.fillSmoothRoundRect(kRescanX + 1, kRescanY + 1, kRescanW - 2, kRescanH - 2,
                           kRescanH / 2 - 1, kBackground, kAccent);
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kAccent, kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(tr(Str::WifiRescan), kRescanX + kRescanW / 2, kRescanY + kRescanH / 2);

  drawStatus(state);
  drawList(state);
  remember(state);
}

void ScreenWifi::update(const UiState& state) {
  const bool statusChanged = state.wifi != lastWifi_ || strcmp(state.ssid, lastSsid_) != 0;
  const uint8_t saved = state.saved != nullptr ? state.saved->count : 0;
  if (statusChanged) {
    drawStatus(state);
  }
  if (statusChanged || state.scanVersion != lastScanVersion_ || saved != lastSavedCount_) {
    if (page_ >= pageCount(state)) {
      page_ = 0;
    }
    drawList(state);
  }
  remember(state);
}

void ScreenWifi::remember(const UiState& state) {
  lastWifi_ = state.wifi;
  strlcpy(lastSsid_, state.ssid, sizeof(lastSsid_));
  lastScanVersion_ = state.scanVersion;
  lastSavedCount_ = state.saved != nullptr ? state.saved->count : 0;
}

uint8_t ScreenWifi::pageCount(const UiState& state) const {
  const uint8_t n = state.scanResults != nullptr ? state.scanResults->count : 0;
  return n == 0 ? 1 : (n + kRowsPerPage - 1) / kRowsPerPage;
}

void ScreenWifi::drawStatus(const UiState& s) {
  tft_.fillRect(0, kStatusMidY - kStatusH / 2, kW, kStatusH, kBackground);
  char buf[64];
  uint16_t color = kMuted;
  if (s.wifi == WifiState::Connected) {
    char name[48];
    snprintf(name, sizeof(name), "%s - %s", s.ssid, s.ip);  // glyph set has no middle dot
    snprintf(buf, sizeof(buf), tr(Str::WifiConnectedFmt), name);
    color = kOk;
  } else if (s.wifi == WifiState::Connecting) {
    snprintf(buf, sizeof(buf), tr(Str::WifiConnectingFmt), s.ssid);
    color = kWarn;
  } else {
    strlcpy(buf, tr(Str::NotConnected), sizeof(buf));
  }
  tft_.setTextColor(color, kBackground);
  tft_.setTextDatum(ML_DATUM);
  ui::drawFitted(tft_, buf, kStatusX, kStatusMidY, kStatusMaxW, kFontSmall, kFontSmall);
}

void ScreenWifi::drawList(const UiState& s) {
  tft_.fillRect(0, kListY, kW, config::kScreenHeight - kListY, kBackground);
  ui::drawCard(tft_, kListX, kListY, kListW, kListH);

  const uint8_t n = s.scanResults != nullptr ? s.scanResults->count : 0;
  if (n == 0) {
    Str msg = Str::WifiNoNetworks;
    if (s.scan == ScanState::Scanning || s.scan == ScanState::Idle) {
      msg = Str::WifiScanning;
    } else if (s.scan == ScanState::Failed) {
      msg = Str::WifiScanFailed;
    }
    useFont(tft_, kFontSmall);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(tr(msg), kListX + kListW / 2, kListY + kListH / 2);
  } else {
    for (int i = 0; i < kRowsPerPage; ++i) {
      const int idx = page_ * kRowsPerPage + i;
      if (idx >= n) {
        break;
      }
      const WifiNetworkInfo& net = s.scanResults->items[idx];
      const int16_t top = kListY + i * kRowH;
      if (i > 0) {
        tft_.drawFastHLine(kListX + 10, top, kListW - 20, kLine);
      }
      const bool saved = s.saved != nullptr && findNetwork(*s.saved, net.ssid) != nullptr;
      drawRow(net, top, isConnectedTo(s, net.ssid), saved);
    }
  }
  drawPager(pageCount(s));
}

void ScreenWifi::drawRow(const WifiNetworkInfo& net, int16_t top, bool connected, bool saved) {
  const int16_t mid = top + kRowH / 2;
  drawBars(tft_, kBarsX, mid + kBarsBottomOffset, barsFor(net.rssi));

  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  ui::drawFitted(tft_, net.ssid, kSsidX, mid, kSsidMaxW, kFontMedium, kFontSmall);

  if (saved && !connected) {
    useFont(tft_, kFontSmall);
    tft_.setTextColor(kMuted, kBackground);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(tr(Str::WifiSaved), kSavedRightX, mid);
    return;
  }
  if (net.secured) {
    drawLock(tft_, kLockX, mid, kMuted);
  }
  if (connected) {
    drawCheck(tft_, kMarkX, mid, kOk);
  } else {
    ui::drawChevron(tft_, kMarkX + 4, mid, true, kMuted, kBackground);
  }
}

void ScreenWifi::drawPager(uint8_t pages) {
  if (pages <= 1) {
    return;  // area already cleared by drawList
  }
  useFont(tft_, kFontSmall);
  const int16_t mid = kPagerY + kPagerH / 2;
  const struct {
    int16_t x;
    Str label;
    bool right;
    bool enabled;
  } buttons[] = {{kPrevX, Str::WifiPrev, false, page_ > 0},
                 {kNextX, Str::WifiNext, true, page_ + 1 < pages}};
  for (const auto& b : buttons) {
    const uint16_t fg = b.enabled ? kInk : kDisabled;
    tft_.fillSmoothRoundRect(b.x, kPagerY, kPagerBtnW, kPagerH, kPagerH / 2, kTrack, kBackground);
    tft_.setTextColor(fg, kTrack);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(tr(b.label), b.x + kPagerBtnW / 2, mid);
    const int16_t chevronX = b.right ? b.x + kPagerBtnW - 16 : b.x + 16;
    ui::drawChevron(tft_, chevronX, mid, b.right, fg, kTrack);
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%u / %u", page_ + 1, pages);
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(buf, kW / 2, mid);
}

WifiHit ScreenWifi::handleTap(int16_t x, int16_t y, const UiState& state) {
  WifiHit hit;
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    hit.target = WifiTarget::Back;
  } else if (ui::hit(x, y, kRescanHitX, 0, kW - kRescanHitX, ui::kBackHitH)) {
    hit.target = WifiTarget::Rescan;
  } else if (ui::hit(x, y, kListX, kListY, kListW, kListH)) {
    const int idx = page_ * kRowsPerPage + (y - kListY) / kRowH;
    const int n = state.scanResults != nullptr ? state.scanResults->count : 0;
    if (idx < n) {
      hit.target = WifiTarget::Network;
      hit.index = static_cast<uint8_t>(idx);
    }
  } else if (y >= kPagerY - kPagerHitPad) {
    const uint8_t pages = pageCount(state);
    if (x < kPrevX + kPagerBtnW + kPagerHitPad && page_ > 0) {
      --page_;
      hit.target = WifiTarget::PrevPage;
    } else if (x >= kNextX - kPagerHitPad && page_ + 1 < pages) {
      ++page_;
      hit.target = WifiTarget::NextPage;
    }
    if (hit.target != WifiTarget::None) {
      drawList(state);
    }
  }
  return hit;
}
