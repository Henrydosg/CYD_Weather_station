#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

namespace {

constexpr char kKeyLanguage[] = "lang";
constexpr char kKeyTouchCal[] = "tcal";
constexpr char kKeyNetworks[] = "nets";
constexpr char kKeyCities[] = "cities";
constexpr char kKeyRotate[] = "rot";
constexpr bool kReadOnly = true;

Settings defaults() {
  Settings s{};
  s.language = static_cast<Language>(config::kDefaultLanguage);
  s.touch = {config::kTouchRawLeft, config::kTouchRawRight, config::kTouchRawTop,
             config::kTouchRawBottom};
  s.touchCalibrated = false;
  s.cities.count = 1;
  City& home = s.cities.items[0];
  strlcpy(home.name, config::kDefaultCityName, sizeof(home.name));
  strlcpy(home.region, config::kDefaultCityRegion, sizeof(home.region));
  home.latitude = config::kDefaultCityLat;
  home.longitude = config::kDefaultCityLon;
  s.rotateSec = config::kDefaultRotateSec;
  return s;
}

bool validRotation(uint8_t sec) {
  for (uint8_t choice : config::kRotateChoicesSec) {
    if (sec == choice) {
      return true;
    }
  }
  return false;
}

}  // namespace

Settings SettingsStore::load() {
  Settings s = defaults();
  Preferences prefs;
  if (!prefs.begin(config::kSettingsNamespace, kReadOnly)) {
    Serial.println("[settings] no stored settings, using defaults");
    return s;
  }

  const uint8_t lang = prefs.getUChar(kKeyLanguage, config::kDefaultLanguage);
  s.language = lang == static_cast<uint8_t>(Language::English) ? Language::English
                                                                 : Language::Vietnamese;
  TouchCalibration cal{};
  if (prefs.getBytes(kKeyTouchCal, &cal, sizeof(cal)) == sizeof(cal)) {
    s.touch = cal;
    s.touchCalibrated = true;
  }
  SavedNetworks nets{};
  if (prefs.getBytes(kKeyNetworks, &nets, sizeof(nets)) == sizeof(nets) &&
      nets.count <= config::kMaxSavedNetworks) {
    s.networks = nets;
  }
  SavedCities cities{};
  if (prefs.getBytes(kKeyCities, &cities, sizeof(cities)) == sizeof(cities) &&
      cities.count >= 1 && cities.count <= config::kMaxCities) {
    s.cities = cities;
  }
  const uint8_t rot = prefs.getUChar(kKeyRotate, config::kDefaultRotateSec);
  s.rotateSec = validRotation(rot) ? rot : config::kDefaultRotateSec;
  prefs.end();

  // Counts only; passwords are never logged.
  Serial.printf("[settings] language %s, touch %s, %u network(s), %u city(ies), rotate %u s\n",
                languageCode(s.language), s.touchCalibrated ? "calibrated" : "default",
                s.networks.count, s.cities.count, s.rotateSec);
  return s;
}

void SettingsStore::save(const Settings& s) {
  Preferences prefs;
  if (!prefs.begin(config::kSettingsNamespace, !kReadOnly)) {
    Serial.println("[settings] NVS open failed");
    return;
  }
  prefs.putUChar(kKeyLanguage, static_cast<uint8_t>(s.language));
  if (s.touchCalibrated) {
    prefs.putBytes(kKeyTouchCal, &s.touch, sizeof(s.touch));
  }
  prefs.putBytes(kKeyNetworks, &s.networks, sizeof(s.networks));
  prefs.putBytes(kKeyCities, &s.cities, sizeof(s.cities));
  prefs.putUChar(kKeyRotate, s.rotateSec);
  prefs.end();
  Serial.println("[settings] saved");
}
