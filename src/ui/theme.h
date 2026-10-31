// Shared UI colours (dark theme). Purely visual constants for src/ui.
#pragma once

#include <stdint.h>

namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBg = rgb(11, 18, 32);
constexpr uint16_t kCard = rgb(22, 33, 58);  // also the icon preview bg in tools/gen_icons.py
constexpr uint16_t kCardLine = rgb(44, 60, 96);
constexpr uint16_t kText = rgb(241, 245, 249);
constexpr uint16_t kMuted = rgb(148, 163, 184);
constexpr uint16_t kAccent = rgb(96, 165, 250);
constexpr uint16_t kOk = rgb(74, 222, 128);
constexpr uint16_t kWarn = rgb(251, 191, 36);
constexpr uint16_t kError = rgb(248, 113, 113);
constexpr uint16_t kSecondHand = rgb(244, 63, 94);

}  // namespace theme
