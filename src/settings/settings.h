// User settings shared between modules as plain data.
#pragma once

#include <stdint.h>

#include "config.h"

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

// WiFi network chosen on the device. The password is stored in plain text in
// NVS (flash encryption is not enabled) and must never be logged.
struct WifiCredential {
  char ssid[config::kMaxSsidLen + 1];
  char password[config::kMaxPasswordLen + 1];  // "" for open networks
};

struct SavedNetworks {
  uint8_t count;
  WifiCredential items[config::kMaxSavedNetworks];  // most recently used first
};

// Moves (or adds) a network to the front; the oldest one drops off when full.
void rememberNetwork(SavedNetworks& nets, const char* ssid, const char* password);
// Removes a network; returns false when it was not saved.
bool forgetNetwork(SavedNetworks& nets, const char* ssid);
// nullptr when not saved.
const WifiCredential* findNetwork(const SavedNetworks& nets, const char* ssid);

struct Settings {
  Language language;
  TouchCalibration touch;
  bool touchCalibrated;  // false until the user has run the calibration once
  SavedNetworks networks;
};
