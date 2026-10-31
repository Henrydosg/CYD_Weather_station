#include "touch_input.h"

#include "config.h"

namespace {

// Linear map of a raw reading to 0..size-1 given the raw values at both edges.
int16_t mapAxis(int32_t raw, int32_t rawAtZero, int32_t rawAtMax, int16_t size) {
  if (rawAtMax == rawAtZero) {
    return 0;
  }
  const int32_t px = (raw - rawAtZero) * (size - 1) / (rawAtMax - rawAtZero);
  return static_cast<int16_t>(constrain(px, 0, size - 1));
}

}  // namespace

TouchInput::TouchInput() : spi_(VSPI), ts_(config::kTouchCs, config::kTouchIrq) {}

void TouchInput::begin() {
  spi_.begin(config::kTouchClk, config::kTouchMiso, config::kTouchMosi, config::kTouchCs);
  ts_.begin(spi_);
  ts_.setRotation(config::kScreenRotation);
}

TouchEvent TouchInput::poll(uint32_t now) {
  TouchEvent ev;
  const bool touched = ts_.touched();
  const bool rising = touched && !wasTouched_;
  wasTouched_ = touched;

  if (!rising || (now - lastTap_) < config::kTouchDebounceMs) {
    return ev;
  }
  lastTap_ = now;

  // Average a few readings to reduce jitter (each is a short SPI transfer).
  int32_t sx = 0, sy = 0;
  for (uint8_t i = 0; i < config::kTouchSamples; ++i) {
    const TS_Point p = ts_.getPoint();
    sx += p.x;
    sy += p.y;
  }
  ev.tapped = true;
  ev.rawX = static_cast<int16_t>(sx / config::kTouchSamples);
  ev.rawY = static_cast<int16_t>(sy / config::kTouchSamples);
  ev.x = mapAxis(ev.rawX, cal_.rawLeft, cal_.rawRight, config::kScreenWidth);
  ev.y = mapAxis(ev.rawY, cal_.rawTop, cal_.rawBottom, config::kScreenHeight);
  Serial.printf("[touch] raw %d,%d -> %d,%d\n", ev.rawX, ev.rawY, ev.x, ev.y);
  return ev;
}
