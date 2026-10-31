// User settings shared between modules as plain data.
#pragma once

#include <stdint.h>

enum class Language : uint8_t {
  Vietnamese = 0,
  English = 1,
};

// OpenWeatherMap "lang" parameter for a UI language.
inline const char* languageCode(Language lang) {
  return lang == Language::English ? "en" : "vi";
}

// Raw XPT2046 readings at the screen edges (x = 0 / width-1, y = 0 / height-1).
// Edges may be "reversed" (left > right) when an axis is mirrored.
struct TouchCalibration {
  int16_t rawLeft;
  int16_t rawRight;
  int16_t rawTop;
  int16_t rawBottom;
};

struct Settings {
  Language language;
  TouchCalibration touch;
  bool touchCalibrated;  // false until the user has run the calibration once
};
