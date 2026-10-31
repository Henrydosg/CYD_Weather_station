#include "analog_clock.h"

#include <math.h>

#include "assets/fonts.h"
#include "theme.h"

namespace {

// Geometry (sprite-local pixels)
constexpr int16_t kFaceSize = 136;
constexpr float kC = kFaceSize / 2.0f;  // centre
constexpr int16_t kRadius = 66;
constexpr int16_t kRingWidth = 3;
constexpr float kHourTickOuter = 60;
constexpr float kHourTickInner = 53;
constexpr float kMinuteDotR = 59;
constexpr float kNumeralR = 44;

constexpr float kHourLen = 33, kHourTail = 6, kHourBase = 3.2f, kHourTip = 2.0f;
constexpr float kMinLen = 49, kMinTail = 8, kMinBase = 2.4f, kMinTip = 1.3f;
constexpr float kSecLen = 54, kSecTail = 13, kSecBase = 1.1f, kSecTip = 0.7f;

constexpr int16_t kTextW = 138;
constexpr int16_t kTextH = 30;

// Hands shown (dimmed) while the clock has not been synchronised.
constexpr int kIdleHour = 10, kIdleMinute = 10;

constexpr float kDegPerHour = 30.0f, kDegPerMinute = 6.0f;

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
  if (valid == lastValid_ && second == lastSecond_) {
    return;
  }
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
                      tipR, color);
}

void AnalogClock::drawFace(const struct tm* t) {
  using namespace theme;
  face_.fillSprite(kBg);

  face_.fillSmoothCircle(kC, kC, kRadius, kCard, kBg);
  face_.drawSmoothArc(kC, kC, kRadius, kRadius - kRingWidth, 0, 360, kCardLine, kBg);

  // Minute dots and hour ticks.
  for (int i = 0; i < 60; ++i) {
    const float rad = i * kDegPerMinute * DEG_TO_RAD;
    const float sx = sinf(rad), sy = -cosf(rad);
    if (i % 5 == 0) {
      const bool quarter = i % 15 == 0;
      face_.drawWedgeLine(kC + sx * kHourTickInner, kC + sy * kHourTickInner,
                          kC + sx * kHourTickOuter, kC + sy * kHourTickOuter,
                          quarter ? 1.6f : 1.0f, quarter ? 1.6f : 1.0f,
                          quarter ? kText : kMuted, kCard);
    } else {
      face_.drawPixel(kC + sx * kMinuteDotR, kC + sy * kMinuteDotR, kCardLine);
    }
  }

  face_.setTextColor(kMuted, kCard);
  face_.setTextDatum(MC_DATUM);
  face_.drawString("12", kC, kC - kNumeralR);
  face_.drawString("3", kC + kNumeralR, kC);
  face_.drawString("6", kC, kC + kNumeralR);
  face_.drawString("9", kC - kNumeralR, kC);

  if (t != nullptr) {
    const float minuteAngle = (t->tm_min + t->tm_sec / 60.0f) * kDegPerMinute;
    const float hourAngle = ((t->tm_hour % 12) + t->tm_min / 60.0f) * kDegPerHour;
    drawHand(hourAngle, kHourLen, kHourTail, kHourBase, kHourTip, kText);
    drawHand(minuteAngle, kMinLen, kMinTail, kMinBase, kMinTip, kText);
    drawHand(t->tm_sec * kDegPerMinute, kSecLen, kSecTail, kSecBase, kSecTip, kSecondHand);
    face_.fillSmoothCircle(kC, kC, 4, kSecondHand);
    face_.fillSmoothCircle(kC, kC, 1, kCard);
  } else {
    drawHand(kIdleHour * kDegPerHour + kIdleMinute / 2.0f, kHourLen, kHourTail, kHourBase,
             kHourTip, kCardLine);
    drawHand(kIdleMinute * kDegPerMinute, kMinLen, kMinTail, kMinBase, kMinTip, kCardLine);
    face_.fillSmoothCircle(kC, kC, 4, kCardLine);
  }

  face_.pushSprite(faceX_, faceY_);
}

void AnalogClock::drawDigital(const struct tm* t) {
  using namespace theme;
  text_.fillSprite(kBg);
  text_.setTextDatum(MC_DATUM);
  char buf[12];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    text_.setTextColor(kText, kBg);
  } else {
    strlcpy(buf, "--:--:--", sizeof(buf));
    text_.setTextColor(kMuted, kBg);
  }
  text_.drawString(buf, kTextW / 2, kTextH / 2);
  text_.pushSprite(textX_, textY_);
}
