// Plain data passed from main.cpp to the UI every loop iteration, and the
// commands the UI sends back.
#pragma once

#include <stdint.h>
#include <time.h>

#include "../net/ota_status.h"
#include "../net/wifi_manager.h"
#include "../photos/photo_library.h"
#include "../settings/settings.h"
#include "../weather/weather_data.h"

struct UiState {
  // WiFi
  WifiState wifi;
  int8_t rssi;              // dBm, 0 when not connected
  const char* ssid;         // network connected or being connected to
  const char* ip;           // dotted IP, "" when not connected
  ScanState scan;
  uint32_t scanVersion;     // changes whenever scan state or results change
  const WifiScanResults* scanResults;
  const SavedNetworks* saved;
  ConnectAttempt attempt;   // result of the last network chosen by the user
  // Cities and their weather (same order, `weather` has cities->count entries)
  const SavedCities* cities;
  uint32_t citiesVersion;   // changes whenever the city list changes
  uint8_t rotateSec;
  const CityWeather* weather;
  FetchStatus fetch;
  uint32_t dataVersion;     // changes whenever any city's weather changes
  // City search
  SearchStatus search;
  uint32_t searchVersion;
  const GeoResult* searchResults;
  uint8_t searchCount;
  // Photos on the SD card (scanned on UiAction::ScanPhotos)
  const PhotoList* photos;
  // Firmware update
  const OtaStatus* ota;
  // Clock
  bool timeValid;
  time_t utcNow;            // valid when timeValid
};

enum class UiAction : uint8_t {
  None,
  LanguageChanged,   // see UiCommand::language
  CalibrationSaved,  // see UiCommand::calibration
  ScanWifi,
  ConnectWifi,       // see UiCommand::ssid / password
  ForgetWifi,        // see UiCommand::ssid
  SetRotation,       // see UiCommand::value (seconds)
  SearchCity,        // see UiCommand::text
  AddCity,           // see UiCommand::index (into the search results)
  MoveCity,          // see UiCommand::index / delta
  RemoveCity,        // see UiCommand::index
  ScanPhotos,        // (re)mount the SD card and list the photos
  CheckUpdate,       // look for a newer firmware now
  InstallUpdate,     // download and install OtaStatus::latest
};

struct UiCommand {
  UiAction action = UiAction::None;
  Language language = Language::Vietnamese;
  TouchCalibration calibration{};
  char ssid[config::kMaxSsidLen + 1] = "";
  char password[config::kMaxPasswordLen + 1] = "";
  char text[config::kMaxSearchLen + 1] = "";
  uint8_t index = 0;
  int8_t delta = 0;
  uint8_t value = 0;
};
