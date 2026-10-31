#include "analog_clock.h"

#include <math.h>

#include "assets/fonts.h"

using namespace theme;

namespace {

// Geometry (sprite-local pixels)
constexpr int16_t kFaceSize = 136;
constexpr float kC = kFaceSize / 2.0f;  // centre
constexpr int16_t kRadius = 66;
constexpr int16_t kRingWidth = 2;
constexpr float kTickOuter = 60;
constexpr float kQuarterTickInner = 50;
constexpr float kHourTickInner = 54;
constexpr float kMinuteDotR = 59;
constexpr float kNumeralR = 40;

constexpr float kHourLen = 32, kHourTail = 0, kHourBase = 2.6f, kHourTip = 2.2f;
constexpr float kMinLen = 48, kMinTail = 0, kMinBase = 1.9f, kMinTip = 1.5f;
constexpr float kSecLen = 55, kSecTail = 12, kSecBase = 0.9f, kSecTip = 0.7f;
constexpr int16_t kHubR = 4, kHubInnerR = 1;

constexpr int16_t kTextW = 138;
constexpr int16_t kTextH = 30;

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

void AnalogClock::setPalette(const Palette& palette) {
  palette_ = &palette;
  dirty_ = true;
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

void AnalogClock::paintSky(TFT_eSprite& sprite, int16_t screenY) {
  for (int16_t y = 0; y < sprite.height(); ++y) {
    sprite.drawFastHLine(0, y, sprite.width(), skyAt(*palette_, screenY + y));
  }
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
  paintSky(face_, faceY_);

  // Translucent face: a white ring and a lighter disc over the sky.
  const uint16_t sky = skyAt(*palette_, faceY_ + kFaceSize / 2);
  const uint16_t face = blend(kCardAlpha, kText, sky);
  const uint16_t minor = blend(kMinorTickAlpha, kMuted, face);
  face_.fillSmoothCircle(kC, kC, kRadius, blend(kRingAlpha, kText, sky), kReadBackground);
  face_.fillSmoothCircle(kC, kC, kRadius - kRingWidth, face, kReadBackground);

  for (int i = 0; i < 60; ++i) {
    const float rad = i * kDegPerMinute * DEG_TO_RAD;
    const float sx = sinf(rad), sy = -cosf(rad);
    if (i % 15 == 0) {
      face_.drawWedgeLine(kC + sx * kQuarterTickInner, kC + sy * kQuarterTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, 1.5f, 1.5f, kText, face);
    } else if (i % 5 == 0) {
      face_.drawWedgeLine(kC + sx * kHourTickInner, kC + sy * kHourTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, 1.0f, 1.0f, minor, face);
    } else {
      face_.drawPixel(kC + sx * kMinuteDotR, kC + sy * kMinuteDotR, minor);
    }
  }

  face_.setTextColor(kMuted, face);
  face_.setTextDatum(MC_DATUM);
  face_.drawString("12", kC, kC - kNumeralR);

  if (t != nullptr) {
    const float minuteAngle = (t->tm_min + t->tm_sec / 60.0f) * kDegPerMinute;
    const float hourAngle = ((t->tm_hour % 12) + t->tm_min / 60.0f) * kDegPerHour;
    drawHand(hourAngle, kHourLen, kHourTail, kHourBase, kHourTip, kText);
    drawHand(minuteAngle, kMinLen, kMinTail, kMinBase, kMinTip, kText);
    drawHand(t->tm_sec * kDegPerMinute, kSecLen, kSecTail, kSecBase, kSecTip, kSecondHand);
    face_.fillSmoothCircle(kC, kC, kHubR, kSecondHand, kReadBackground);
    face_.fillSmoothCircle(kC, kC, kHubInnerR, face, kReadBackground);
  } else {
    drawHand(kIdleHour * kDegPerHour + kIdleMinute / 2.0f, kHourLen, kHourTail, kHourBase,
             kHourTip, minor);
    drawHand(kIdleMinute * kDegPerMinute, kMinLen, kMinTail, kMinBase, kMinTip, minor);
    face_.fillSmoothCircle(kC, kC, kHubR, minor, kReadBackground);
  }

  face_.pushSprite(faceX_, faceY_);
}

void AnalogClock::drawDigital(const struct tm* t) {
  paintSky(text_, textY_);
  const uint16_t sky = skyAt(*palette_, textY_ + kTextH / 2);
  text_.setTextDatum(MC_DATUM);
  char buf[12];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    text_.setTextColor(kText, sky);
  } else {
    strlcpy(buf, "--:--:--", sizeof(buf));
    text_.setTextColor(kMuted, sky);
  }
  text_.drawString(buf, kTextW / 2, kTextH / 2);
  text_.pushSprite(textX_, textY_);
}
