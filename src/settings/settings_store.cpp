#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

namespace {

constexpr char kKeyLanguage[] = "lang";
constexpr char kKeyTouchCal[] = "tcal";
constexpr char kKeyNetworks[] = "nets";
constexpr char kKeyCities[] = "cities2";  // City with names in both languages
constexpr char kKeyCitiesV1[] = "cities";  // older layout, one name per city
constexpr char kKeyRotate[] = "rot";
constexpr char kKeyPanelGamma[] = "pgam";
constexpr char kKeyPhotoTone[] = "ptone";
constexpr bool kReadOnly = true;

constexpr uint8_t kVi = static_cast<uint8_t>(Language::Vietnamese);
constexpr uint8_t kEn = static_cast<uint8_t>(Language::English);

// Layout of kKeyCitiesV1 (name in the UI language of the moment it was added).
struct CityV1 {
  char name[config::kMaxCityNameLen + 1];
  char region[config::kMaxCityRegionLen + 1];
  float latitude;
  float longitude;
};
struct SavedCitiesV1 {
  uint8_t count;
  CityV1 items[config::kMaxCities];
};

Settings defaults() {
  Settings s{};
  s.language = static_cast<Language>(config::kDefaultLanguage);
  s.touch = {config::kTouchRawLeft, config::kTouchRawRight, config::kTouchRawTop,
             config::kTouchRawBottom};
  s.touchCalibrated = false;
  s.cities.count = 1;
  City& home = s.cities.items[0];
  home.geoId = config::kDefaultCityGeoId;
  strlcpy(home.name[kVi], config::kDefaultCityNameVi, sizeof(home.name[kVi]));
  strlcpy(home.name[kEn], config::kDefaultCityNameEn, sizeof(home.name[kEn]));
  strlcpy(home.region[kVi], config::kDefaultCityRegionVi, sizeof(home.region[kVi]));
  strlcpy(home.region[kEn], config::kDefaultCityRegionEn, sizeof(home.region[kEn]));
  home.latitude = config::kDefaultCityLat;
  home.longitude = config::kDefaultCityLon;
  s.rotateSec = config::kDefaultRotateSec;
  s.panelGamma = static_cast<PanelGamma>(config::kDefaultPanelGamma);
  s.photoTone = config::kDefaultPhotoTone;
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

// Old cities keep their one name in the language of the moment; the other
// language (and the GeoNames id) are looked up once the network is up.
bool loadCitiesV1(Preferences& prefs, Language lang, SavedCities& out) {
  static SavedCitiesV1 old;  // static: keeps ~0.8 KB off the setup() stack
  old = SavedCitiesV1{};
  if (prefs.getBytes(kKeyCitiesV1, &old, sizeof(old)) != sizeof(old) || old.count < 1 ||
      old.count > config::kMaxCities) {
    return false;
  }
  const uint8_t l = static_cast<uint8_t>(lang);
  out = SavedCities{};
  out.count = old.count;
  for (uint8_t i = 0; i < old.count; ++i) {
    City& c = out.items[i];
    c.geoId = kGeoIdUnknown;
    strlcpy(c.name[l], old.items[i].name, sizeof(c.name[l]));
    strlcpy(c.region[l], old.items[i].region, sizeof(c.region[l]));
    c.latitude = old.items[i].latitude;
    c.longitude = old.items[i].longitude;
  }
  Serial.printf("[settings] migrated %u city(ies) from the old layout\n", out.count);
  return true;
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
  static SavedCities cities;  // static: keeps ~1.6 KB off the setup() stack
  cities = SavedCities{};
  if (prefs.getBytes(kKeyCities, &cities, sizeof(cities)) == sizeof(cities) &&
      cities.count >= 1 && cities.count <= config::kMaxCities) {
    s.cities = cities;
  } else if (loadCitiesV1(prefs, s.language, cities)) {
    s.cities = cities;
  }
  const uint8_t rot = prefs.getUChar(kKeyRotate, config::kDefaultRotateSec);
  s.rotateSec = validRotation(rot) ? rot : config::kDefaultRotateSec;
  const uint8_t gamma = prefs.getUChar(kKeyPanelGamma, config::kDefaultPanelGamma);
  s.panelGamma = static_cast<PanelGamma>(
      gamma < static_cast<uint8_t>(PanelGamma::Count) ? gamma : config::kDefaultPanelGamma);
  const uint8_t tone = prefs.getUChar(kKeyPhotoTone, config::kDefaultPhotoTone);
  s.photoTone = tone < config::kPhotoToneLevels ? tone : config::kDefaultPhotoTone;
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
  if (prefs.isKey(kKeyCitiesV1)) {
    prefs.remove(kKeyCitiesV1);  // migrated on load
  }
  prefs.putUChar(kKeyRotate, s.rotateSec);
  prefs.putUChar(kKeyPanelGamma, static_cast<uint8_t>(s.panelGamma));
  prefs.putUChar(kKeyPhotoTone, s.photoTone);
  prefs.end();
  Serial.println("[settings] saved");
}
