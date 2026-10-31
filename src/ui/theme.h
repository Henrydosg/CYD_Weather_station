// Black dashboard theme (user's choice, like the classic CYD weather screens):
// pure black background, white values, magenta labels, cyan description and
// lows, yellow time / temperature / highs, orange "feels like". Colours are fixed.
// Purely visual constants for src/ui; tools/preview_screen.py parses the
// rgb(...) lines below.
#pragma once

#include <stdint.h>

namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBackground = rgb(0, 0, 0);       // screen and card fill
constexpr uint16_t kInk = rgb(255, 255, 255);        // titles and values
constexpr uint16_t kMuted = rgb(190, 196, 212);      // secondary text
constexpr uint16_t kLabel = rgb(255, 100, 255);      // "label:" column
constexpr uint16_t kAccent = rgb(40, 140, 255);      // buttons, pills, links
constexpr uint16_t kOnAccent = rgb(255, 255, 255);   // text on kAccent
constexpr uint16_t kCardBorder = rgb(64, 68, 86);
constexpr uint16_t kLine = rgb(44, 46, 60);          // separators
constexpr uint16_t kTrack = rgb(30, 32, 44);         // keys, selected row, today
constexpr uint16_t kDisabled = rgb(100, 104, 118);
constexpr uint16_t kHighlight = rgb(255, 232, 80);  // time, active dot, calibration target
constexpr uint16_t kDescription = rgb(100, 228, 255);
constexpr uint16_t kTemp = rgb(255, 232, 80);       // current temperature
constexpr uint16_t kFeels = rgb(255, 176, 90);      // "feels like"
constexpr uint16_t kHigh = rgb(255, 228, 90);        // daily maximum
constexpr uint16_t kLow = rgb(90, 210, 255);        // daily minimum, rain chance
constexpr uint16_t kGraph = rgb(255, 232, 80);
constexpr uint16_t kRainBar = rgb(50, 120, 220);
constexpr uint16_t kOk = rgb(120, 235, 160);
constexpr uint16_t kWarn = rgb(255, 214, 102);
constexpr uint16_t kAlert = rgb(255, 150, 80);
constexpr uint16_t kError = rgb(255, 110, 100);
constexpr uint16_t kSevere = rgb(200, 120, 255);
constexpr uint16_t kDanger = rgb(200, 60, 60);       // destructive button
constexpr uint16_t kBarOff = rgb(60, 62, 76);       // unlit signal bars

// Current temperature colour: cool blue .. yellow .. orange .. red.
uint16_t temperatureColor(float celsius);
// UV index (0..11+) and US AQI levels (see strings uvLevelName / aqiLevelName).
uint8_t uvLevel(float uv);
uint8_t aqiLevel(int aqi);
uint16_t levelColor(uint8_t level);  // 0 good .. 5 hazardous

}  // namespace theme
