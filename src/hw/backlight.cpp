#include "backlight.h"

#include <Arduino.h>

#include "config.h"

namespace {
constexpr uint8_t kFullPct = 100;
constexpr uint32_t kMaxDuty = (1UL << config::kBacklightPwmBits) - 1;
}  // namespace

void Backlight::begin() {
  ledcSetup(config::kBacklightPwmChannel, config::kBacklightPwmHz, config::kBacklightPwmBits);
  ledcAttachPin(config::kBacklightPin, config::kBacklightPwmChannel);
  analogSetPinAttenuation(config::kLdrPin, ADC_0db);  // the LDR divider gives small voltages
  write(kFullPct);
}

void Backlight::write(uint8_t pct) {
  pct_ = pct;
  ledcWrite(config::kBacklightPwmChannel, kMaxDuty * pct / kFullPct);
}

uint8_t Backlight::targetPct(Brightness mode) const {
  switch (mode) {
    case Brightness::High:
      return config::kBacklightHighPct;
    case Brightness::Medium:
      return config::kBacklightMediumPct;
    case Brightness::Low:
      return config::kBacklightLowPct;
    case Brightness::Auto:
      break;
  }
  if (ambient_ < 0) {
    return kFullPct;
  }
  // 0 = dark .. 1 = bright; works whichever way the sensor reading runs.
  float light = (ambient_ - config::kLdrDarkRaw) /
                (static_cast<float>(config::kLdrBrightRaw) - config::kLdrDarkRaw);
  light = constrain(light, 0.0f, 1.0f);
  return config::kBacklightAutoMinPct +
         static_cast<uint8_t>(light * (kFullPct - config::kBacklightAutoMinPct) + 0.5f);
}

void Backlight::update(uint32_t now, Brightness mode) {
  if (now - lastSampleAt_ < config::kLightSampleMs) {
    return;
  }
  lastSampleAt_ = now;
  const uint16_t raw = analogRead(config::kLdrPin);
  ambient_ = ambient_ < 0 ? raw : ambient_ + config::kLightSmoothing * (raw - ambient_);

  const uint8_t target = targetPct(mode);
  if (target != pct_) {
    // Fixed levels apply at once (a tap in Settings); Auto fades.
    uint8_t next = target;
    if (mode == Brightness::Auto) {
      const int step = config::kBacklightFadeStepPct;
      next = target > pct_ ? min<int>(target, pct_ + step) : max<int>(target, pct_ - step);
    }
    write(next);
  }
  if (mode == Brightness::Auto && config::kLightLogMs != 0 &&
      now - lastLogAt_ >= config::kLightLogMs) {
    lastLogAt_ = now;
    Serial.printf("[light] raw=%u smoothed=%.0f backlight=%u%%\n", raw, ambient_, pct_);
  }
}
