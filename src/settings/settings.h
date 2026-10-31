// User settings shared between modules as plain data.
#pragma once

#include <stdint.h>

#include "config.h"

enum class Language : uint8_t {
  Vietnamese = 0,
  English = 1,
};

// Open-Meteo geocoding "language" parameter for a UI language.
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

// City shown on the main screen (found with the geocoding search).
struct City {
  char name[config::kMaxCityNameLen + 1];      // UTF-8, e.g. "Đà Lạt"
  char region[config::kMaxCityRegionLen + 1];  // "admin1, country"
  float latitude;
  float longitude;
};

struct SavedCities {
  uint8_t count;
  City items[config::kMaxCities];  // rotation order
};

// Adds at the end; false when full or already saved (same coordinates).
bool addCity(SavedCities& cities, const City& city);
// Swaps city `index` with its neighbour (delta -1 = up, +1 = down).
bool moveCity(SavedCities& cities, uint8_t index, int8_t delta);
bool removeCity(SavedCities& cities, uint8_t index);

struct Settings {
  Language language;
  TouchCalibration touch;
  bool touchCalibrated;  // false until the user has run the calibration once
  SavedNetworks networks;
  SavedCities cities;    // never empty (defaults to config::kDefaultCity*)
  uint8_t rotateSec;     // one of config::kRotateChoicesSec
};
