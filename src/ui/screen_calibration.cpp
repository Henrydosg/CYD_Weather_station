#include "screen_calibration.h"

#include <stdlib.h>

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kH = config::kScreenHeight;
constexpr int16_t kInset = config::kCalTargetInset;
constexpr int16_t kX1 = kInset, kY1 = kInset;
constexpr int16_t kX2 = kW - 1 - kInset, kY2 = kH - 1 - kInset;

constexpr int16_t kArm = ui::lay::S(12);        // crosshair half length
constexpr int16_t kRingR = ui::lay::S(7);
constexpr float kStroke = ui::lay::Sf(0.8f);
constexpr int16_t kMessageY = kH / 2;
constexpr int16_t kStepY = kMessageY + 24;

// Raw value at screen pixel 0 and size-1 from two readings at known pixels.
void solveAxis(int16_t rawA, int16_t rawB, int16_t pxA, int16_t pxB, int16_t size,
               int16_t& rawAtZero, int16_t& rawAtMax) {
  const float rawPerPx = static_cast<float>(rawB - rawA) / (pxB - pxA);
  rawAtZero = static_cast<int16_t>(lroundf(rawA - rawPerPx * pxA));
  rawAtMax = static_cast<int16_t>(lroundf(rawA + rawPerPx * (size - 1 - pxA)));
}

}  // namespace

void ScreenCalibration::start() {
  step_ = Step::First;
  drawStep(tr(Str::CalTapTarget));
}

void ScreenCalibration::drawTarget(int16_t x, int16_t y, uint16_t color) {
  tft_.drawWedgeLine(x - kArm, y, x + kArm, y, kStroke, kStroke, color, kBackground);
  tft_.drawWedgeLine(x, y - kArm, x, y + kArm, kStroke, kStroke, color, kBackground);
  tft_.drawSmoothCircle(x, y, kRingR, color, kBackground);
}

void ScreenCalibration::drawStep(const char* message) {
  tft_.fillScreen(kBackground);
  ui::useFont(tft_, kFontSmall);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(kInk, kBackground);
  tft_.drawString(message, kW / 2, kMessageY);

  if (step_ == Step::Done) {
    return;
  }
  const bool first = step_ == Step::First;
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(first ? "1 / 2" : "2 / 2", kW / 2, kStepY);
  drawTarget(first ? kX1 : kX2, first ? kY1 : kY2, kHighlight);
}

void ScreenCalibration::handleTap(const TouchEvent& ev, uint32_t now) {
  if (!ev.tapped) {
    return;
  }
  switch (step_) {
    case Step::First:
      raw1X_ = ev.rawX;
      raw1Y_ = ev.rawY;
      step_ = Step::Second;
      drawStep(tr(Str::CalTapTarget));
      break;

    case Step::Second: {
      // Reject taps that are too close together (e.g. double tap on target 1).
      if (abs(ev.rawX - raw1X_) < config::kCalMinRawSpan ||
          abs(ev.rawY - raw1Y_) < config::kCalMinRawSpan) {
        Serial.println("[touch] calibration rejected, retry");
        step_ = Step::First;
        drawStep(tr(Str::CalRetry));
        break;
      }
      solveAxis(raw1X_, ev.rawX, kX1, kX2, kW, result_.rawLeft, result_.rawRight);
      solveAxis(raw1Y_, ev.rawY, kY1, kY2, kH, result_.rawTop, result_.rawBottom);
      Serial.printf("[touch] calibration L%d R%d T%d B%d\n", result_.rawLeft, result_.rawRight,
                    result_.rawTop, result_.rawBottom);
      step_ = Step::Done;
      doneAt_ = now;
      drawStep(tr(Str::CalSaved));
      break;
    }

    case Step::Done:
      break;
  }
}

bool ScreenCalibration::finished(uint32_t now) const {
  return step_ == Step::Done && (now - doneAt_) >= config::kCalDoneMessageMs;
}
