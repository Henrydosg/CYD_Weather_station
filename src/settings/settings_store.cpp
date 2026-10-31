#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

constexpr char kKeyLanguage[] = "lang";
constexpr char kKeyTouchCal[] = "tcal";
constexpr char kKeyNetworks[] = "nets";
constexpr bool kReadOnly = true;

Settings defaults() {
  Settings s{};
  s.language = static_cast<Language>(config::kDefaultLanguage);
  s.touch = {config::kTouchRawLeft, config::kTouchRawRight, config::kTouchRawTop,
             config::kTouchRawBottom};
  s.touchCalibrated = false;
  return s;
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
  prefs.end();

  // Network count only; passwords are never logged.
  Serial.printf("[settings] language %s, touch %s, %u saved network(s)\n",
                languageCode(s.language), s.touchCalibrated ? "calibrated" : "default",
                s.networks.count);
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
  prefs.end();
  Serial.println("[settings] saved");
}
