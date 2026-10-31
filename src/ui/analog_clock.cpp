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

// Digital readout, inside the glass time card drawn by ScreenMain.
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

void AnalogClock::paintRows(TFT_eSprite& sprite, int16_t screenY, uint8_t glassAlpha) {
  for (int16_t y = 0; y < sprite.height(); ++y) {
    sprite.drawFastHLine(0, y, sprite.width(), glassAt(*palette_, screenY + y, glassAlpha));
  }
}

void AnalogClock::fillGlassDisc(float radius, uint16_t under) {
  // Row by row so the glass follows the sky gradient; edge pixels are
  // blended with `under` (the ring) by their coverage.
  const float outerR = radius + 0.5f, innerR = radius - 0.5f;
  for (int16_t y = 0; y < kFaceSize; ++y) {
    const float dy = y + 0.5f - kC;
    if (fabsf(dy) >= outerR) {
      continue;
    }
    const uint16_t glass = glassAt(*palette_, faceY_ + y);
    const float outer = sqrtf(outerR * outerR - dy * dy);
    const float inner = fabsf(dy) < innerR ? sqrtf(innerR * innerR - dy * dy) : 0.0f;
    const int16_t xa = static_cast<int16_t>(ceilf(kC - inner - 0.5f));
    const int16_t xb = static_cast<int16_t>(floorf(kC + inner - 0.5f));
    if (inner > 0.0f && xb >= xa) {
      face_.drawFastHLine(xa, y, xb - xa + 1, glass);
    }
    const int16_t first = static_cast<int16_t>(floorf(kC - outer - 0.5f));
    const int16_t last = static_cast<int16_t>(ceilf(kC + outer - 0.5f));
    for (int16_t x = first; x <= last; ++x) {
      if (inner > 0.0f && x >= xa && x <= xb) {
        x = xb;  // skip the filled span
        continue;
      }
      const float dx = x + 0.5f - kC;
      const float cover = radius + 0.5f - sqrtf(dx * dx + dy * dy);
      if (cover > 0.0f) {
        face_.drawPixel(x, y, blend(static_cast<uint8_t>(fminf(cover, 1.0f) * 255), glass, under));
      }
    }
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
  paintRows(face_, faceY_, 0);

  // Dark glass face with a light ring.
  const uint16_t face = glassAt(*palette_, faceY_ + kFaceSize / 2);
  const uint16_t minor = blend(kMinorTickAlpha, kMuted, face);
  const uint16_t ring = blend(kRingAlpha, kText, face);
  face_.fillSmoothCircle(kC, kC, kRadius, ring, kReadBackground);
  fillGlassDisc(kRadius - kRingWidth, ring);

  for (int i = 0; i < 60; ++i) {
    const float rad = i * kDegPerMinute * DEG_TO_RAD;
    const float sx = sinf(rad), sy = -cosf(rad);
    if (i % 15 == 0) {
      face_.drawWedgeLine(kC + sx * kQuarterTickInner, kC + sy * kQuarterTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, 1.5f, 1.5f, kText,
                          kReadBackground);
    } else if (i % 5 == 0) {
      face_.drawWedgeLine(kC + sx * kHourTickInner, kC + sy * kHourTickInner,
                          kC + sx * kTickOuter, kC + sy * kTickOuter, 1.0f, 1.0f, minor,
                          kReadBackground);
    } else {
      face_.drawPixel(kC + sx * kMinuteDotR, kC + sy * kMinuteDotR, minor);
    }
  }

  face_.setTextColor(kMuted, glassAt(*palette_, faceY_ + static_cast<int16_t>(kC - kNumeralR)));
  face_.setTextDatum(MC_DATUM);
  face_.drawString("12", kC, kC - kNumeralR);

  if (t != nullptr) {
    const float minuteAngle = (t->tm_min + t->tm_sec / 60.0f) * kDegPerMinute;
    const float hourAngle = ((t->tm_hour % 12) + t->tm_min / 60.0f) * kDegPerHour;
    drawHand(hourAngle, kHourLen, kHourTail, kHourBase, kHourTip, kText);
    drawHand(minuteAngle, kMinLen, kMinTail, kMinBase, kMinTip, kText);
    drawHand(t->tm_sec * kDegPerMinute, kSecLen, kSecTail, kSecBase, kSecTip, kSecondHand);
    face_.fillSmoothCircle(kC, kC, kHubR, kSecondHand, kReadBackground);
    face_.fillSmoothCircle(kC, kC, kHubInnerR, glassAt(*palette_, faceY_ + kFaceSize / 2),
                           kReadBackground);
  } else {
    drawHand(kIdleHour * kDegPerHour + kIdleMinute / 2.0f, kHourLen, kHourTail, kHourBase,
             kHourTip, minor);
    drawHand(kIdleMinute * kDegPerMinute, kMinLen, kMinTail, kMinBase, kMinTip, minor);
    face_.fillSmoothCircle(kC, kC, kHubR, minor, kReadBackground);
  }

  face_.pushSprite(faceX_, faceY_);
}

void AnalogClock::drawDigital(const struct tm* t) {
  paintRows(text_, textY_, kGlassAlpha);
  const uint16_t glass = glassAt(*palette_, textY_ + kTextH / 2);
  text_.setTextDatum(MC_DATUM);
  char buf[12];
  if (t != nullptr) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    text_.setTextColor(kText, glass);
  } else {
    strlcpy(buf, "--:--:--", sizeof(buf));
    text_.setTextColor(kMuted, glass);
  }
  text_.drawString(buf, kTextW / 2, kTextH / 2);
  text_.pushSprite(textX_, textY_);
}
