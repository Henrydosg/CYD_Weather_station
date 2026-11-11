// All HTTPS requests (weather, air quality, city search, city names, OTA,
// remote message) run here, on a FreeRTOS task of their own on core 1, so a
// slow or stalled request never holds up loop(): the UI keeps drawing and
// reading taps. Like main.cpp this is wiring: it owns the network clients.
//
// loop() talks to the task only through this class, under one mutex:
//  - inputs (WiFi up, scan running, clock mode) and commands go in,
//  - sync() copies the published results (NetSnapshot) out.
// The task holds the mutex only to take commands and publish, never during a
// request.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#include "config.h"
#include "net/ota_status.h"
#include "net/remote_message.h"
#include "settings/settings.h"
#include "weather/weather_data.h"

// What loop() sees of the network clients (a copy, updated by sync()).
struct NetSnapshot {
  CityWeather weather[config::kMaxCities];
  uint8_t weatherCount = 0;
  FetchStatus fetch = FetchStatus::NoData;
  uint32_t weatherVersion = 0;
  uint32_t weatherStamp = 0;   // changes whenever weather[] is republished
  uint32_t lastWeatherMs = 0;  // millis() of the last successful fetch

  SearchStatus search = SearchStatus::Idle;
  uint32_t searchVersion = 0;
  uint8_t searchCount = 0;
  GeoResult results[config::kGeoMaxResults];

  OtaStatus ota;
  bool otaDownloading = false;
  RemoteMessage message{};

  // The city list with the names CityNameResolver filled in (merge by
  // coordinates: the list in loop() may have changed meanwhile).
  uint32_t namesVersion = 0;
  SavedCities namedCities{};
};

class NetWorker {
 public:
  // Starts the task; false if it (or its mutex) could not be created.
  bool begin(const SavedCities& cities);

  // --- loop() side; none of these waits for a request --------------------------
  void setInputs(bool wifiUp, bool scanning, bool clockMode);
  void setCities(const SavedCities& cities);
  void search(const char* query, const char* langCode);
  void refreshWeather();
  void checkUpdate();
  void installUpdate();
  // WiFi scans must not overlap a request. holdIfIdle() returns true and keeps
  // the task from starting a pass while no pass runs; the caller may then start
  // a scan and must call release() with whether one is now busy. False = a pass
  // runs (nothing held): don't start a scan now.
  bool holdIfIdle();
  void release(bool scanning);
  // Copies what changed since the last call into out.
  void sync(NetSnapshot& out);

 private:
  struct Commands {
    bool cities = false;
    SavedCities citiesNew{};
    bool search = false;
    char query[config::kMaxSearchLen + 1] = "";
    const char* lang = "vi";
    bool refresh = false, check = false, install = false;
  };

  static void taskEntry(void* self);
  void run();
  void pass(uint32_t now);
  void publish(bool citiesChanged);

  SemaphoreHandle_t lock_ = nullptr;
  // Shared (under lock_).
  Commands cmd_;
  bool wifiUp_ = false, scanning_ = false, clockMode_ = false;
  bool busy_ = false;  // a pass (maybe a request) is running
  NetSnapshot shared_;
  // Task side only.
  SavedCities cities_{};
};
