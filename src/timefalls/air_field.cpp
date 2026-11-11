#include "air_field.h"

#include <math.h>

namespace tf {

namespace {

constexpr float kTwoPi = 6.2831853f;
constexpr float kWaveLenMin = 110.0f, kWaveLenMax = 260.0f;  // px
constexpr float kWaveSpeedMax = 0.35f;                        // rad / s
constexpr int kHues = 3;
constexpr uint32_t kHueRgb[kHues] = {0x8FF0E8, 0xC8B8FF, 0xFFF0D0};  // teal, lavender, ivory
constexpr uint32_t kBackgroundRgb = 0x07111D;
// 0.18 + 0.42 was invisible on the inverted TN panel, 0.50 + 0.50 (150 motes)
// read as noise: fewer motes, a little dimmer.
constexpr float kAlphaMin = 0.38f, kAlphaSpeed = 0.45f, kTailShare = 0.45f;
constexpr float kSpeedNorm = 30.0f;  // px/s for full brightness

uint16_t blend565(uint32_t fg, float a) {
  const auto ch = [&](int shift) {
    const float f = static_cast<float>((fg >> shift) & 0xFF);
    const float b = static_cast<float>((kBackgroundRgb >> shift) & 0xFF);
    return static_cast<uint32_t>(b + (f - b) * a + 0.5f);
  };
  const uint32_t r = ch(16), g = ch(8), b = ch(0);
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

}  // namespace

float AirField::rand01() {
  uint32_t x = rng_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_ = x;
  return static_cast<float>(x >> 8) / 16777216.0f;
}

void AirField::begin(uint32_t seed) {
  rng_ = seed != 0 ? seed : 1;
  // Stream function psi = sum a sin(k.p + w t + phase); velocity = curl psi.
  for (Wave& w : waves_) {
    const float len = kWaveLenMin + (kWaveLenMax - kWaveLenMin) * rand01();
    const float dir = kTwoPi * rand01();
    const float k = kTwoPi / len;
    w.kx = k * cosf(dir);
    w.ky = k * sinf(dir);
    w.w = kWaveSpeedMax * (2.0f * rand01() - 1.0f);
    w.phase = kTwoPi * rand01();
    w.amp = kAirSpeed / k / kAirWaves * (0.6f + 0.8f * rand01());
  }
  for (AirMote& m : motes_) {
    spawn(m);
    m.age = m.life * rand01();  // not all born at once
  }
}

void AirField::spawn(AirMote& m) {
  m.x = kSceneW * rand01();
  m.y = kSceneH * rand01();
  m.age = 0;
  m.life = kAirLifeMin + (kAirLifeMax - kAirLifeMin) * rand01();
  m.hue = static_cast<uint8_t>(rand01() * kHues) % kHues;
  m.speed = 0;
}

void AirField::velocity(float x, float y, float t, float& u, float& v) const {
  u = 0;
  v = -kAirRise;
  for (const Wave& w : waves_) {
    const float c = w.amp * cosf(w.kx * x + w.ky * y + w.w * t + w.phase);
    u += c * w.ky;   //  d psi / dy
    v -= c * w.kx;   // -d psi / dx
  }
}

void AirField::update(float t, float dt) {
  gain_ = 1.0f;
  if (gusting_) {
    const float since = t - gustAt_;
    gain_ += kAirGust * expf(-since / kAirGustSeconds);
    if (since > 5.0f * kAirGustSeconds) {
      gusting_ = false;
    }
  }
  for (AirMote& m : motes_) {
    m.age += dt;
    if (m.age >= m.life) {
      spawn(m);
    }
    // Midpoint step: smoother curves than plain Euler.
    float u, v;
    velocity(m.x, m.y, t, u, v);
    float u2, v2;
    velocity(m.x + u * gain_ * dt * 0.5f, m.y + v * gain_ * dt * 0.5f, t, u2, v2);
    m.x += u2 * gain_ * dt;
    m.y += v2 * gain_ * dt;
    m.speed = sqrtf(u2 * u2 + v2 * v2) * gain_;
    // Wrap around the region.
    if (m.x < 0) m.x += kSceneW;
    if (m.x >= kSceneW) m.x -= kSceneW;
    if (m.y < 0) m.y += kSceneH;
    if (m.y >= kSceneH) m.y -= kSceneH;
  }
}

uint16_t AirField::colour(int i, bool tail) const {
  const AirMote& m = motes_[i];
  float fade = m.age / kAirFadeSeconds;
  const float out = (m.life - m.age) / kAirFadeSeconds;
  if (out < fade) fade = out;
  if (fade > 1.0f) fade = 1.0f;
  if (fade < 0.0f) fade = 0.0f;
  float s = m.speed / kSpeedNorm;
  if (s > 1.0f) s = 1.0f;
  float a = (kAlphaMin + kAlphaSpeed * s) * fade;
  if (tail) a *= kTailShare;
  return blend565(kHueRgb[m.hue], a);
}

}  // namespace tf
