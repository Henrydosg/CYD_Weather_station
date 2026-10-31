// Light theme: pure white background, ocean-blue text, white cards with a thin
// blue outline. The colours are fixed (they do not follow weather or time).
// Purely visual constants for src/ui; tools/preview_screen.py parses the
// rgb(...) lines below.
#pragma once

#include <stdint.h>

namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBackground = rgb(255, 255, 255);  // screen and card fill
constexpr uint16_t kInk = rgb(12, 52, 120);           // main text, clock hands
constexpr uint16_t kAccent = rgb(0, 102, 204);        // description, date, pill, clock ring
constexpr uint16_t kMuted = rgb(84, 110, 150);        // labels, footer
constexpr uint16_t kCardBorder = rgb(190, 212, 240);
constexpr uint16_t kLine = rgb(220, 230, 242);        // row separators
constexpr uint16_t kTrack = rgb(214, 226, 242);       // empty part of bars
constexpr uint16_t kClockMinor = rgb(150, 172, 205);  // minor ticks, "12"
constexpr uint16_t kHighlight = rgb(234, 88, 12);     // second hand, calibration target
constexpr uint16_t kOnAccent = rgb(255, 255, 255);    // text on kAccent
constexpr uint16_t kOk = rgb(22, 163, 74);
constexpr uint16_t kWarn = rgb(217, 119, 6);
constexpr uint16_t kError = rgb(220, 38, 38);
constexpr uint16_t kBarOff = rgb(203, 213, 225);      // unlit signal bars

// Temperature colour: blue (cool) .. ink .. orange .. red (hot).
uint16_t temperatureColor(float celsius);

}  // namespace theme
