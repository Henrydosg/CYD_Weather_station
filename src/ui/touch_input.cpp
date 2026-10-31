#include "touch_input.h"

#include "config.h"

TouchInput::TouchInput() : spi_(VSPI), ts_(config::kTouchCs, config::kTouchIrq) {}

void TouchInput::begin() {
  spi_.begin(config::kTouchClk, config::kTouchMiso, config::kTouchMosi, config::kTouchCs);
  ts_.begin(spi_);
  ts_.setRotation(config::kScreenRotation);
}

bool TouchInput::tapped(uint32_t now) {
  const bool touched = ts_.touched();
  const bool rising = touched && !wasTouched_;
  wasTouched_ = touched;

  if (rising && (now - lastTap_) >= config::kTouchDebounceMs) {
    lastTap_ = now;
    return true;
  }
  return false;
}
