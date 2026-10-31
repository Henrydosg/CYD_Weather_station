// User settings shared between modules as plain data.
#pragma once

#include <stdint.h>

#include "config.h"

enum class Language : uint8_t {
  Vietnamese = 0,
  English = 1,
};
constexpr uint8_t kLanguageCount = 2;

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

// GeoNames id of a city that was saved before ids were stored (looked up by
// name and coordinates), and of one the geocoder could not match at all.
constexpr uint32_t kGeoIdUnknown = 0;
constexpr uint32_t kGeoIdNone = 0xFFFFFFFF;

// City shown on the main screen (found with the geocoding search). Names are
// kept in both UI languages, indexed by Language; "" = not looked up yet.
struct City {
  uint32_t geoId;
  char name[kLanguageCount][config::kMaxCityNameLen + 1];      // UTF-8, e.g. "Đà Lạt"
  char region[kLanguageCount][config::kMaxCityRegionLen + 1];  // "admin1, country"
  float latitude;
  float longitude;
};

// Name / region in `lang`, or in the other language while it is not known yet.
const char* cityName(const City& city, Language lang);
const char* cityRegion(const City& city, Language lang);
// True while a name in one of the languages is still missing.
bool cityNeedsNames(const City& city);

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
