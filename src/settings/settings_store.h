// Persists Settings in NVS (ESP32 Preferences). Survives power cycles and
// firmware uploads (unless the flash is erased).
#pragma once

#include "settings.h"

class SettingsStore {
 public:
  // Loads stored values, falling back to defaults from config.h.
  Settings load();
  void save(const Settings& settings);
};
