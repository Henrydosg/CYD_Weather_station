// CYD (ESP32-2432S028) weather station - entry point.
// Wiring of the modules only; all logic lives in src/net, src/weather,
// src/settings and src/ui. The HTTPS clients run on the network task
// (net_worker), the touch panel on its own polling task: loop() never waits
// for a request.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "hw/backlight.h"
#include "net/remote_message.h"
#include "net/time_sync.h"
#include "net/wifi_manager.h"
#include "photos/photo_library.h"
#include "settings/settings_store.h"
#include "sys/boot_health.h"
#include "timefalls/hourglass_app.h"
#include "ui/touch_input.h"
#include "net_worker.h"
#include "ui/ui_controller.h"

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
static NetWorker net;
static NetSnapshot netView;  // the network task's results, copied each loop
static uint32_t namesSeen = 0;
static PhotoLibrary photos;
static Backlight backlight;
static BootHealth boot;
static char macText[18] = "";  // station MAC, for the Info screen

// TIME FALLS: the hourglass clock is the home screen (its own task on core 0);
// the weather station opens from its weather block and returns to it.
static tf::HourglassApp hourglass(tft);
static bool clockMode = false;         // the hourglass has the display
static bool toWeatherPending = false;  // waiting for the hourglass to let go of it
static bool toSettings = false;        // ... then open Settings, not the dashboard
static uint32_t lastClockShare = 0;
static uint32_t messageOpened = 0;     // remote message id that opened the weather UI
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
  net.setCities(settings.cities);
  store.save(settings);
}

// Names the resolver filled in (on the network task's copy of the list) go
// into the saved list; cities are matched by coordinates, since the list may
// have changed meanwhile.
static void mergeCityNames() {
  if (netView.namesVersion == namesSeen) {
    return;
  }
  namesSeen = netView.namesVersion;
  bool changed = false;
  const SavedCities& named = netView.namedCities;
  for (uint8_t i = 0; i < settings.cities.count; ++i) {
    City& city = settings.cities.items[i];
    for (uint8_t j = 0; j < named.count; ++j) {
      const City& n = named.items[j];
      if (n.latitude == city.latitude && n.longitude == city.longitude &&
          memcmp(&n, &city, sizeof(City)) != 0) {
        city = n;
        changed = true;
        break;
      }
    }
  }
  if (changed) {
    ++citiesVersion;
    store.save(settings);
  }
}

// The weather the clock shows: the cities in turn (Settings rotation time).
static void shareWeather(const UiState& state, uint32_t now) {
  if (now - lastClockShare < tf::kColumnRefreshMs) {
    return;
  }
  lastClockShare = now;
  static tf::ClockWeather w;  // static: CityWeather is large for the stack
  w = tf::ClockWeather{};
  w.wifiUp = wifi.isConnected();
  const uint8_t count = settings.cities.count;
  if (count > 0 && state.weather != nullptr) {
    const uint32_t slot = settings.rotateSec > 0 ? now / (settings.rotateSec * 1000UL) : 0;
    const uint8_t i = static_cast<uint8_t>(slot % count);
    if (state.weather[i].valid) {
      w.valid = true;
      w.weather = state.weather[i];
      strlcpy(w.city, cityName(settings.cities.items[i], settings.language), sizeof(w.city));
    }
  }
  w.fetching = state.fetch == FetchStatus::Fetching;
  hourglass.setWeather(w);
}

// The hourglass has the display: taps go to it; its weather block, its gear
// (Settings) or a new remote message hands the display to the weather UI.
static void runClockMode(const UiState& state, const TouchEvent& tap, uint32_t now) {
  if (tap.tapped) {
    hourglass.tap(tap.x, tap.y);
  }
  shareWeather(state, now);
  const bool newMessage = state.message != nullptr && state.message->id != messageOpened;
  const bool wantSettings = hourglass.takeSettingsRequest();
  if (!toWeatherPending && (wantSettings || hourglass.takeWeatherRequest() || newMessage)) {
    if (newMessage) {
      messageOpened = state.message->id;
    }
    toSettings = wantSettings && !newMessage;  // a message shows over the dashboard
    hourglass.setActive(false);
    toWeatherPending = true;
  }
  if (toWeatherPending && hourglass.displayReleased()) {
    toWeatherPending = false;
    clockMode = false;
    if (netView.weatherVersion == 0 || now - netView.lastWeatherMs >= config::kWeatherRefreshMs) {
      net.refreshWeather();  // fresh data for the full weather screens
    }
    ui.enter(now, toSettings);
  }
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
    case UiAction::SetBrightness:
      settings.brightness = static_cast<Brightness>(cmd.value % kBrightnessCount);
      store.save(settings);
      break;
    case UiAction::MessageRead:
      settings.messageSeen = cmd.id;
      store.save(settings);
      break;
    case UiAction::SearchCity:
      net.search(cmd.text, languageCode(settings.language));
      break;
    case UiAction::AddCity:
      if (cmd.index < netView.searchCount) {
        const GeoResult& r = netView.results[cmd.index];
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
      net.checkUpdate();
      break;
    case UiAction::InstallUpdate:
      net.installUpdate();
      break;
    case UiAction::ShowClock:  // handled in loop()
    case UiAction::None:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[main] CYD weather station v%s starting\n", config::kFirmwareVersion);
  boot.begin();
  // Restart if loop() hangs (the core's watchdog only watches the idle task).
  esp_task_wdt_init(config::kLoopWatchdogSec, true);
  esp_task_wdt_add(nullptr);

  settings = store.load();

  tft.init();
  backlight.begin();
  touch.begin();
  touch.setCalibration(settings.touch);
  if (!touch.startTask()) {
    Serial.println("[main] touch task not started, polling from loop()");
  }
  ui.begin(settings);
  // Calibration first (weather UI); otherwise the hourglass is the home screen.
  const bool clockFirst = settings.touchCalibrated;
  clockMode = hourglass.begin(clockFirst) && clockFirst;
  wifi.setSavedNetworks(&settings.networks);
  wifi.begin(millis());
  if (!net.begin(settings.cities)) {
    Serial.println("[main] network task not started");
  }
  strlcpy(macText, WiFi.macAddress().c_str(), sizeof(macText));
}

void loop() {
  esp_task_wdt_reset();
  // loop() shares core 1 with the network task at the same priority: without a
  // pause it would take half the CPU just spinning and slow every TLS handshake
  // down (they are cut off at kTlsHandshakeSec).
  vTaskDelay(pdMS_TO_TICKS(config::kLoopYieldMs));
  const uint32_t now = millis();

  // A WiFi scan may start only while the network task is between requests.
  const bool netIdle = net.holdIfIdle();
  wifi.setScanAllowed(netIdle);
  wifi.update(now);
  if (netIdle) {
    net.release(wifi.scanBusy());
  }
  timeSync.update(wifi.isConnected());
  net.setInputs(wifi.isConnected(), wifi.scanBusy(), clockMode);
  net.sync(netView);
  mergeCityNames();
  backlight.update(now, settings.brightness);
  onConnectAttempt(wifi.attempt());
  boot.update(now, wifi.isConnected() && netView.weatherVersion > 0);

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
  state.brightness = settings.brightness;
  state.backlightPct = backlight.percent();
  state.weather = netView.weather;
  state.fetch = netView.fetch;
  state.dataVersion = netView.weatherVersion;
  state.search = netView.search;
  state.searchVersion = netView.searchVersion;
  state.searchResults = netView.results;
  state.searchCount = netView.searchCount;
  state.photos = &photos.list();
  state.ota = &netView.ota;
  state.system.boot = &boot.report();
  state.system.uptimeSec = now / 1000;
  state.system.freeHeap = ESP.getFreeHeap();
  state.system.minFreeHeap = ESP.getMinFreeHeap();
  state.system.mac = macText;
  state.system.weatherAgeSec =
      netView.weatherVersion > 0 ? static_cast<int32_t>((now - netView.lastWeatherMs) / 1000) : -1;
  state.timeValid = timeSync.isValid();
  state.utcNow = time(nullptr);
  const RemoteMessage& message = netView.message;
  state.message = state.timeValid && messageDue(message, settings.messageSeen,
                                                static_cast<uint32_t>(state.utcNow))
                      ? &message
                      : nullptr;

  const TouchEvent tap = touch.take(now);
  if (clockMode) {
    runClockMode(state, tap, now);
    return;
  }
  const UiCommand cmd = ui.update(state, tap, now);
  if (cmd.action == UiAction::ShowClock) {
    clockMode = true;
    hourglass.setActive(true);  // the weather UI does not draw from here on
    return;
  }
  apply(cmd, now);
}
