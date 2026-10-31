#include "analog_clock.h"

#include <math.h>

#include "assets/fonts.h"
#include "theme.h"

using namespace theme;

namespace {

// Geometry (sprite-local pixels)
constexpr int16_t kFaceSize = 136;
constexpr float kC = kFaceSize / 2.0f;  // centre
constexpr int16_t kRadius = 66;
constexpr int16_t kRingWidth = 2;
constexpr float kQuarterTickR = 2.0f, kHourTickR = 1.0f;
constexpr float kTickOuter = 60;
constexpr float kQuarterTickInner = 50;
constexpr float kHourTickInner = 54;
constexpr float kMinuteDotR = 59;
constexpr float kNumeralR = 40;

constexpr float kHourLen = 32, kHourTail = 0, kHourBase = 3.0f, kHourTip = 2.6f;
constexpr float kMinLen = 48, kMinTail = 0, kMinBase = 2.0f, kMinTip = 1.6f;
constexpr float kSecLen = 55, kSecTail = 12, kSecBase = 1.0f, kSecTip = 0.8f;
constexpr int16_t kHubR = 4, kHubInnerR = 1;

// Digital readout, inside the time card drawn by ScreenMain.
constexpr int16_t kTextW = AnalogClock::kDigitalW;
constexpr int16_t kTextH = AnalogClock::kDigitalH;

// Hands shown (dimmed) while the clock has not been synchronised.
constexpr int kIdleHour = 10, kIdleMinute = 10;

constexpr float kDegPerHour = 30.0f, kDegPerMinute = 6.0f;
constexpr uint32_t kReadBackground = 0x00FFFFFF;  // TFT_eSPI: blend with existing pixels

}  // namespace

AnalogClock::AnalogClock(TFT_eSPI& tft, int16_t faceX, int16_t faceY, int16_t textX, int16_t textY)
    : tft_(tft), face_(&tft), text_(&tft), faceX_(faceX), faceY_(faceY), textX_(textX),
      textY_(textY) {}

void AnalogClock::begin() {
  face_.setColorDepth(16);
  text_.setColorDepth(16);
  const bool ok = face_.createSprite(kFaceSize, kFaceSize) != nullptr &&
                  text_.createSprite(kTextW, kTextH) != nullptr;
  if (!ok) {
    Serial.println("[clock] sprite allocation failed");
    return;
  }
  face_.loadFont(kFontSmall);
  text_.loadFont(kFontClock);
  ready_ = true;
}

void AnalogClock::update(const struct tm* t) {
  if (!ready_) {
    return;
  }
  const bool valid = t != nullptr;
  const int second = valid ? t->tm_sec : -1;
  if (!dirty_ && valid == lastValid_ && second == lastSecond_) {
    return;
  }
  dirty_ = false;
  lastValid_ = valid;
  lastSecond_ = second;

  drawFace(t);
  drawDigital(t);
}

void AnalogClock::drawHand(float angleDeg, float length, float tail, float baseR, float tipR,
                           uint16_t color) {
  // 0 deg = 12 o'clock, clockwise.
  const float rad = angleDeg * DEG_TO_RAD;
  const float dx = sinf(rad), dy = -cosf(rad);
  face_.drawWedgeLine(kC - dx * tail, kC - dy * tail, kC + dx * length, kC + dy * length, baseR,
                      tipR, color, kReadBackground);
}

void AnalogClock::drawFace(const struct tm* t) {
  // White face with a thin accent ring.
  face_.fillSprite(kBackground);
  face_.fillSmoothCircle(kC, kC, kRadius, kAccent, kBackground);
  face_.fillSmoothCircle(kC, kC, kRadius - kRingWidth, kBackground, kAccent);

  for (int i = 0; i < 60; ++i) {
    const float rad = i * kDegPerMinute * DEG_TO_RAD;
    const float sx = sinf(rad), sy = -cosf(rad);
    if (i % 15 == 0) {
      face_.drawWedgeLine(kC + sx * kQuarterTickInner, kC + sy * kQuarterTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, kQuarterTickR,
                          kQuarterTickR, kInk, kBackground);
    } else if (i % 5 == 0) {
      face_.drawWedgeLine(kC + sx * kHourTickInner, kC + sy * kHourTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, kHourTickR, kHourTickR,
                          kClockMinor, kBackground);
    } else {
      face_.drawPixel(kC + sx * kMinuteDotR, kC + sy * kMinuteDotR, kClockMinor);
    }
  }

  face_.setTextColor(kClockMinor, kBackground);
  face_.setTextDatum(MC_DATUM);
  face_.drawString("12", kC, kC - kNumeralR);

  if (t != nullptr) {
    const float minuteAngle = (t->tm_min + t->tm_sec / 60.0f) * kDegPerMinute;
    const float hourAngle = ((t->tm_hour % 12) + t->tm_min / 60.0f) * kDegPerHour;
    drawHand(hourAngle, kHourLen, kHourTail, kHourBase, kHourTip, kInk);
    drawHand(minuteAngle, kMinLen, kMinTail, kMinBase, kMinTip, kInk);
    drawHand(t->tm_sec * kDegPerMinute, kSecLen, kSecTail, kSecBase, kSecTip, kHighlight);
    face_.fillSmoothCircle(kC, kC, kHubR, kHighlight, kReadBackground);
    face_.fillSmoothCircle(kC, kC, kHubInnerR, kBackground, kReadBackground);
  } else {
    drawHand(kIdleHour * kDegPerHour + kIdleMinute / 2.0f, kHourLen, kHourTail, kHourBase,
             kHourTip, kClockMinor);
    drawHand(kIdleMinute * kDegPerMinute, kMinLen, kMinTail, kMinBase, kMinTip, kClockMinor);
    face_.fillSmoothCircle(kC, kC, kHubR, kClockMinor, kReadBackground);
  }

  face_.pushSprite(faceX_, faceY_);
}

void AnalogClock::drawDigital(const struct tm* t) {
  text_.fillSprite(kBackground);
  text_.setTextDatum(MC_DATUM);
  char buf[12];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    text_.setTextColor(kInk, kBackground);
  } else {
    strlcpy(buf, "--:--:--", sizeof(buf));
    text_.setTextColor(kClockMinor, kBackground);
  }
  text_.drawString(buf, kTextW / 2, kTextH / 2);
  text_.pushSprite(textX_, textY_);
}
