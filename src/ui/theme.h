// "Dynamic sky" theme: a saturated three-stop gradient that follows the
// weather and the time of day (dawn, day, dusk, night), with dark glass cards
// on top. Purely visual constants for src/ui.
#pragma once

#include <stdint.h>
#include <time.h>

#include "../weather/weather_data.h"

namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Blend fg over bg; alpha 0..255 (255 = fg). Same maths as TFT_eSPI::alphaBlend.
uint16_t blend(uint8_t alpha, uint16_t fg, uint16_t bg);

// Fixed colours (readable on every sky palette and on dark glass).
constexpr uint16_t kText = rgb(255, 255, 255);
constexpr uint16_t kMuted = rgb(210, 225, 244);
constexpr uint16_t kOk = rgb(120, 235, 160);
constexpr uint16_t kWarn = rgb(255, 214, 102);
constexpr uint16_t kError = rgb(255, 120, 110);
constexpr uint16_t kBarFill = rgb(110, 205, 255);
constexpr uint16_t kSecondHand = rgb(255, 184, 77);
constexpr uint16_t kGlassTint = rgb(0, 0, 0);

// Dark glass: black at this alpha over the sky, row by row.
constexpr uint8_t kGlassAlpha = 92;        // ~36 %, cards and clock face
constexpr uint8_t kFooterGlassAlpha = 150; // ~59 %, small text near light gradient bottoms
constexpr uint8_t kRimAlpha = 60;          // white highlight on the top edge of a card
// Translucent-white overlays.
constexpr uint8_t kLineAlpha = 45;         // separators, over the glass colour
constexpr uint8_t kRingAlpha = 110;        // clock ring, over the glass colour
constexpr uint8_t kMinorTickAlpha = 120;
constexpr uint8_t kTrackAlpha = 50;        // empty part of the humidity bar

struct Palette {
  uint16_t skyTop;     // kept dark: header text sits directly on it
  uint16_t skyMid;     // at row kSkyMidRow, the most saturated stop
  uint16_t skyBottom;
  uint16_t accent;     // weather description
};

enum class DayPhase : uint8_t { Dawn, Day, Dusk, Night };

const Palette& paletteFor(WeatherCategory category, DayPhase phase);
// Palette for the current data and local time (nullptr when unknown). Falls
// back to WeatherData::isDay without a clock or sun times, and to
// defaultPalette() without data.
const Palette& paletteFor(const WeatherData* data, const struct tm* now);
const Palette& defaultPalette();

// Sky colour of screen row y (0 = top) for a full-screen vertical gradient.
// Every module paints the background with this, so redrawn regions match.
uint16_t skyAt(const Palette& p, int16_t y);
// Dark glass colour of screen row y.
uint16_t glassAt(const Palette& p, int16_t y, uint8_t alpha = kGlassAlpha);

// Temperature colour: cool blue .. white .. orange .. red.
uint16_t temperatureColor(float celsius);

}  // namespace theme
