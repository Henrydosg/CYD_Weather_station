#include "touch_input.h"

#include <Arduino.h>

#include "config.h"

namespace {

// XPT2046 control bytes (12-bit, differential). The same sequence as the
// XPT2046_Touchscreen library used before, so stored calibrations stay valid.
constexpr uint8_t kCmdZ1 = 0xB1;
constexpr uint16_t kCmdZ2 = 0xC1;
constexpr uint16_t kCmdX = 0x91;
constexpr uint16_t kCmdY = 0xD1;
constexpr uint16_t kCmdYPowerDown = 0xD0;
constexpr int kResultShift = 3;  // 12-bit result in bits 14..3
constexpr int32_t kRawMax = 4095;
constexpr int32_t kPressureThreshold = 300;

// Linear map of a raw reading to 0..size-1 given the raw values at both edges.
int16_t mapAxis(int32_t raw, int32_t rawAtZero, int32_t rawAtMax, int16_t size) {
  if (rawAtMax == rawAtZero) {
    return 0;
  }
  const int32_t px = (raw - rawAtZero) * (size - 1) / (rawAtMax - rawAtZero);
  return static_cast<int16_t>(constrain(px, 0, size - 1));
}

// Mean of the two closest of three readings.
int16_t bestTwoAverage(int16_t a, int16_t b, int16_t c) {
  const int16_t ab = abs(a - b), ac = abs(a - c), bc = abs(b - c);
  if (ab <= ac && ab <= bc) {
    return (a + b) / 2;
  }
  return ac <= bc ? (a + c) / 2 : (b + c) / 2;
}

}  // namespace

void TouchInput::begin() {
  pinMode(config::kTouchClk, OUTPUT);
  pinMode(config::kTouchMosi, OUTPUT);
  pinMode(config::kTouchCs, OUTPUT);
  pinMode(config::kTouchMiso, INPUT);
  pinMode(config::kTouchIrq, INPUT);
  digitalWrite(config::kTouchClk, LOW);
  digitalWrite(config::kTouchCs, HIGH);
}

// SPI mode 0, MSB first; digitalWrite keeps the clock well below the
// controller's 2.5 MHz limit.
uint8_t TouchInput::transfer(uint8_t out) {
  uint8_t in = 0;
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(config::kTouchMosi, (out >> bit) & 1);
    digitalWrite(config::kTouchClk, HIGH);
    in = static_cast<uint8_t>((in << 1) | digitalRead(config::kTouchMiso));
    digitalWrite(config::kTouchClk, LOW);
  }
  return in;
}

uint16_t TouchInput::transfer16(uint16_t out) {
  const uint8_t hi = transfer(static_cast<uint8_t>(out >> 8));
  const uint8_t lo = transfer(static_cast<uint8_t>(out & 0xFF));
  return static_cast<uint16_t>((hi << 8) | lo);
}

bool TouchInput::read(int16_t& rawX, int16_t& rawY) {
  int16_t d[6] = {};
  digitalWrite(config::kTouchCs, LOW);
  // Each transfer returns the result of the previous command.
  transfer(kCmdZ1);
  const int32_t z1 = transfer16(kCmdZ2) >> kResultShift;
  const int32_t z2 = transfer16(kCmdX) >> kResultShift;
  const int32_t z = z1 + kRawMax - z2;
  const bool pressed = z >= kPressureThreshold;
  if (pressed) {
    transfer16(kCmdX);  // the first position reading is noisy
    d[0] = transfer16(kCmdY) >> kResultShift;
    d[1] = transfer16(kCmdX) >> kResultShift;
    d[2] = transfer16(kCmdY) >> kResultShift;
    d[3] = transfer16(kCmdX) >> kResultShift;
  }
  d[4] = transfer16(kCmdYPowerDown) >> kResultShift;
  d[5] = transfer16(0) >> kResultShift;
  digitalWrite(config::kTouchCs, HIGH);
  if (!pressed) {
    return false;
  }
  rawX = bestTwoAverage(d[0], d[2], d[4]);
  rawY = bestTwoAverage(d[1], d[3], d[5]);
  return true;
}

TouchEvent TouchInput::poll(uint32_t now) {
  TouchEvent ev;
  int16_t x = 0, y = 0;
  // IRQ is low while the panel is pressed; skip the SPI reads otherwise.
  const bool touched = digitalRead(config::kTouchIrq) == LOW && read(x, y);
  const bool rising = touched && !wasTouched_;
  wasTouched_ = touched;

  if (!rising || (now - lastTap_) < config::kTouchDebounceMs) {
    return ev;
  }
  lastTap_ = now;

  // Average a few readings to reduce jitter.
  int32_t sx = x, sy = y;
  uint8_t n = 1;
  for (uint8_t i = 1; i < config::kTouchSamples; ++i) {
    if (read(x, y)) {
      sx += x;
      sy += y;
      ++n;
    }
  }
  ev.tapped = true;
  ev.rawX = static_cast<int16_t>(sx / n);
  ev.rawY = static_cast<int16_t>(sy / n);
  ev.x = mapAxis(ev.rawX, cal_.rawLeft, cal_.rawRight, config::kScreenWidth);
  ev.y = mapAxis(ev.rawY, cal_.rawTop, cal_.rawBottom, config::kScreenHeight);
  Serial.printf("[touch] raw %d,%d -> %d,%d\n", ev.rawX, ev.rawY, ev.x, ev.y);
  return ev;
}
