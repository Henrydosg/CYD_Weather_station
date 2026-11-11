#include "screen_connect.h"

#include "assets/fonts.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kTitleMaxW = ui::lay::X(280);

constexpr int16_t kCardX = ui::lay::X(30);
constexpr int16_t kCardY = ui::lay::Y(70);
constexpr int16_t kCardW = kW - 60;
constexpr int16_t kCardH = ui::lay::Y(110);
constexpr int16_t kLine1Y = kCardY + 28, kLine2Y = kCardY + 50;
constexpr int16_t kMessageY = kCardY + kCardH / 2;

constexpr int16_t kBtnY = kCardY + 70;
constexpr int16_t kBtnH = ui::lay::Y(28);
constexpr int16_t kBtnW = ui::lay::X(104);
constexpr int16_t kBtnGap = ui::lay::X(12);
constexpr int16_t kBtnPairX = kW / 2 - kBtnW - kBtnGap / 2;  // left of two buttons
constexpr int16_t kBtnSingleX = kW / 2 - kBtnW / 2;
constexpr int16_t kBtnHitPad = ui::lay::S(6);

}  // namespace

void ScreenConnect::start(const char* ssid, bool secured, bool saved) {
  strlcpy(ssid_, ssid, sizeof(ssid_));
  secured_ = secured;
  saved_ = saved;
  phase_ = Phase::Waiting;
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, ssid_, kTitleMaxW);
  draw();
}

void ScreenConnect::update(const UiState& state, uint32_t now) {
  Phase next = phase_;
  switch (state.attempt) {
    case ConnectAttempt::Pending:
      next = Phase::Pending;
      break;
    case ConnectAttempt::Succeeded:
    case ConnectAttempt::Failed:
      // Ignore the result of an earlier attempt until ours has started.
      if (phase_ == Phase::Pending) {
        next = state.attempt == ConnectAttempt::Succeeded ? Phase::Succeeded : Phase::Failed;
      }
      break;
    case ConnectAttempt::None:
      break;
  }
  if (next != phase_) {
    phase_ = next;
    doneAt_ = now;
    if (phase_ != Phase::Pending) {
      draw();
    }
  }
}

void ScreenConnect::draw() {
  ui::drawCard(tft_, kCardX, kCardY, kCardW, kCardH);
  useFont(tft_, kFontSmall);
  tft_.setTextDatum(MC_DATUM);
  const int16_t cx = kCardX + kCardW / 2;

  if (phase_ != Phase::Failed) {
    const bool ok = phase_ == Phase::Succeeded;
    useFont(tft_, kFontMedium);
    tft_.setTextColor(ok ? kOk : kAccent, kBackground);
    tft_.drawString(tr(ok ? Str::WifiConnectedOk : Str::WifiConnecting), cx, kMessageY);
    return;
  }

  tft_.setTextColor(kError, kBackground);
  tft_.drawString(tr(Str::WifiFailedLine1), cx, kLine1Y);
  tft_.drawString(tr(Str::WifiFailedLine2), cx, kLine2Y);

  const bool both = hasRetry() && hasForget();
  int16_t x = both ? kBtnPairX : kBtnSingleX;
  if (hasRetry()) {
    tft_.fillSmoothRoundRect(x, kBtnY, kBtnW, kBtnH, kBtnH / 2, kAccent, kBackground);
    tft_.setTextColor(kOnAccent, kAccent);
    tft_.drawString(tr(Str::WifiRetry), x + kBtnW / 2, kBtnY + kBtnH / 2);
    x += kBtnW + kBtnGap;
  }
  if (hasForget()) {
    tft_.fillSmoothRoundRect(x, kBtnY, kBtnW, kBtnH, kBtnH / 2, kAccent, kBackground);
    tft_.fillSmoothRoundRect(x + 1, kBtnY + 1, kBtnW - 2, kBtnH - 2, kBtnH / 2 - 1, kBackground,
                             kAccent);
    tft_.setTextColor(kAccent, kBackground);
    tft_.drawString(tr(Str::WifiForget), x + kBtnW / 2, kBtnY + kBtnH / 2);
  }
}

ConnectTarget ScreenConnect::hitTest(int16_t x, int16_t y) const {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return ConnectTarget::Back;
  }
  if (phase_ != Phase::Failed ||
      !ui::hit(x, y, 0, kBtnY - kBtnHitPad, kW, kBtnH + 2 * kBtnHitPad)) {
    return ConnectTarget::None;
  }
  if (hasRetry() && hasForget()) {
    return x < kW / 2 ? ConnectTarget::Retry : ConnectTarget::Forget;
  }
  if (x < kBtnSingleX - kBtnHitPad || x >= kBtnSingleX + kBtnW + kBtnHitPad) {
    return ConnectTarget::None;
  }
  return hasRetry() ? ConnectTarget::Retry : (hasForget() ? ConnectTarget::Forget
                                                          : ConnectTarget::None);
}

bool ScreenConnect::finished(uint32_t now) const {
  return phase_ == Phase::Succeeded && now - doneAt_ >= config::kWifiDoneMessageMs;
}
