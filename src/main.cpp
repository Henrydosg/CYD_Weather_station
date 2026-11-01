// CYD (ESP32-2432S028) weather station - entry point.
// Wiring of the modules only; all logic lives in src/net, src/weather,
// src/settings and src/ui.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "net/ota_updater.h"
#include "net/time_sync.h"
#include "net/wifi_manager.h"
#include "photos/photo_library.h"
#include "settings/settings_store.h"
#include "ui/touch_input.h"
#include "ui/ui_controller.h"
#include "weather/city_names.h"
#include "weather/geocoding.h"
#include "weather/open_meteo.h"

// Release builds (env cyd-release, published for OTA) carry no WiFi password:
// the device uses the networks saved on its WiFi screen.
#if defined(RELEASE_BUILD)
namespace secrets {
constexpr char kWifiSsid[] = "";
constexpr char kWifiPassword[] = "";
}  // namespace secrets
#elif __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h - copy include/secrets.example.h and fill in your WiFi credentials"
#endif

static TFT_eSPI tft;
static UiController ui(tft);
static TouchInput touch;
static SettingsStore store;
static Settings settings;
static WifiManager wifi(secrets::kWifiSsid, secrets::kWifiPassword);
static TimeSync timeSync;
static OpenMeteoClient weather;
static GeocodingClient geocoder;
static CityNameResolver cityNames;
static PhotoLibrary photos;
static OtaUpdater ota;
static uint32_t citiesVersion = 1;

// Network the user asked for; remembered in NVS once the connect succeeds.
static WifiCredential pendingNetwork{};
static ConnectAttempt lastAttempt = ConnectAttempt::None;

static void onConnectAttempt(ConnectAttempt attempt) {
  if (attempt == lastAttempt) {
    return;
  }
  lastAttempt = attempt;
  if (attempt == ConnectAttempt::Succeeded && pendingNetwork.ssid[0] != '\0') {
    rememberNetwork(settings.networks, pendingNetwork.ssid, pendingNetwork.password);
    store.save(settings);
  }
  if (attempt != ConnectAttempt::Pending) {
    memset(&pendingNetwork, 0, sizeof(pendingNetwork));  // don't keep the password around
  }
}

// Saves the city list and tells the weather client (keeps known data).
static void citiesChanged() {
  ++citiesVersion;
  weather.setCities(settings.cities);
  store.save(settings);
}

static void apply(const UiCommand& cmd, uint32_t now) {
  switch (cmd.action) {
    case UiAction::LanguageChanged:
      settings.language = cmd.language;
      store.save(settings);
      break;
    case UiAction::CalibrationSaved:
      settings.touch = cmd.calibration;
      settings.touchCalibrated = true;
      touch.setCalibration(cmd.calibration);
      store.save(settings);
      break;
    case UiAction::ScanWifi:
      wifi.startScan();
      break;
    case UiAction::ConnectWifi:
      strlcpy(pendingNetwork.ssid, cmd.ssid, sizeof(pendingNetwork.ssid));
      strlcpy(pendingNetwork.password, cmd.password, sizeof(pendingNetwork.password));
      wifi.connectTo(cmd.ssid, cmd.password, now);
      break;
    case UiAction::ForgetWifi:
      if (forgetNetwork(settings.networks, cmd.ssid)) {
        store.save(settings);
      }
      break;
    case UiAction::SetRotation:
      settings.rotateSec = cmd.value;
      store.save(settings);
      break;
    case UiAction::SearchCity:
      geocoder.search(cmd.text, languageCode(settings.language));
      break;
    case UiAction::AddCity:
      if (cmd.index < geocoder.count()) {
        const GeoResult& r = geocoder.results()[cmd.index];
        // Name in the search language; cityNames fetches the other one.
        const uint8_t lang = static_cast<uint8_t>(settings.language);
        City city{};
        city.geoId = r.id;
        strlcpy(city.name[lang], r.name, sizeof(city.name[lang]));
        strlcpy(city.region[lang], r.region, sizeof(city.region[lang]));
        city.latitude = r.latitude;
        city.longitude = r.longitude;
        if (addCity(settings.cities, city)) {
          citiesChanged();
        }
      }
      break;
    case UiAction::MoveCity:
      if (moveCity(settings.cities, cmd.index, cmd.delta)) {
        citiesChanged();
      }
      break;
    case UiAction::RemoveCity:
      if (removeCity(settings.cities, cmd.index)) {
        citiesChanged();
      }
      break;
    case UiAction::ScanPhotos:
      photos.scan();
      break;
    case UiAction::CheckUpdate:
      ota.requestCheck();
      break;
    case UiAction::InstallUpdate:
      ota.requestInstall();
      break;
    case UiAction::None:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[main] CYD weather station v%s starting\n", config::kFirmwareVersion);

  settings = store.load();
  weather.setCities(settings.cities);

  tft.init();
  touch.begin();
  touch.setCalibration(settings.touch);
  ui.begin(settings);
  wifi.setSavedNetworks(&settings.networks);
  wifi.begin(millis());
}

void loop() {
  const uint32_t now = millis();

  wifi.update(now);
  timeSync.update(wifi.isConnected());
  // While a firmware download runs, nothing else uses the network (RAM for TLS).
  const bool netFree = wifi.isConnected() && !ota.downloading();
  weather.update(now, netFree);
  geocoder.update(netFree);
  // Name lookups only when no weather fetch or search runs in this iteration.
  const bool otherRequest = weather.status() == FetchStatus::Fetching ||
                            geocoder.status() == SearchStatus::Searching;
  if (cityNames.update(now, netFree && !otherRequest, settings.cities)) {
    ++citiesVersion;
    store.save(settings);
  }
  ota.update(now, wifi.isConnected(), otherRequest);
  onConnectAttempt(wifi.attempt());

  UiState state{};
  state.wifi = wifi.state();
  state.rssi = wifi.rssi();
  state.ssid = wifi.ssid();
  state.ip = wifi.ip();
  state.scan = wifi.scanState();
  state.scanVersion = wifi.scanVersion();
  state.scanResults = &wifi.scanResults();
  state.saved = &settings.networks;
  state.attempt = wifi.attempt();
  state.cities = &settings.cities;
  state.citiesVersion = citiesVersion;
  state.rotateSec = settings.rotateSec;
  state.weather = weather.cities();
  state.fetch = weather.status();
  state.dataVersion = weather.version();
  state.search = geocoder.status();
  state.searchVersion = geocoder.version();
  state.searchResults = geocoder.results();
  state.searchCount = geocoder.count();
  state.photos = &photos.list();
  state.ota = &ota.status();
  state.timeValid = timeSync.isValid();
  state.utcNow = time(nullptr);

  apply(ui.update(state, touch.poll(now), now), now);
}
