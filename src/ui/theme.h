// "Dynamic sky" theme: the background gradient and accent colour follow the
// current weather and day/night. Purely visual constants for src/ui.
#pragma once

#include <stdint.h>

#include "../weather/weather_data.h"

namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Blend fg over bg; alpha 0..255 (255 = fg). Same maths as TFT_eSPI::alphaBlend.
uint16_t blend(uint8_t alpha, uint16_t fg, uint16_t bg);

// Fixed colours (readable on every sky palette).
constexpr uint16_t kText = rgb(255, 255, 255);
constexpr uint16_t kMuted = rgb(210, 225, 244);
constexpr uint16_t kOk = rgb(120, 235, 160);
constexpr uint16_t kWarn = rgb(255, 214, 102);
constexpr uint16_t kError = rgb(255, 120, 110);
constexpr uint16_t kBarFill = rgb(110, 205, 255);
constexpr uint16_t kSecondHand = rgb(255, 184, 77);

// Translucent-white overlays, as alpha over the sky colour underneath.
constexpr uint8_t kCardAlpha = 40;      // ~16 %
constexpr uint8_t kLineAlpha = 45;      // separators, over the card colour
constexpr uint8_t kRingAlpha = 90;      // clock ring, over the sky
constexpr uint8_t kMinorTickAlpha = 120;
constexpr uint8_t kTrackAlpha = 50;     // empty part of the humidity bar

struct Palette {
  uint16_t skyTop;
  uint16_t skyBottom;
  uint16_t accent;  // weather description
};

const Palette& paletteFor(WeatherCategory category, bool isDay);

// Sky colour of screen row y (0 = top) for a full-screen vertical gradient.
// Every module paints the background with this, so redrawn regions match.
uint16_t skyAt(const Palette& p, int16_t y);
const Palette& defaultPalette();

// Temperature colour: cool blue .. white .. orange .. red.
uint16_t temperatureColor(float celsius);

}  // namespace theme
