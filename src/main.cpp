// CYD (ESP32-2432S028) weather station - entry point.
// Wiring of the modules only; all logic lives in src/net, src/weather,
// src/settings and src/ui.
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "net/time_sync.h"
#include "net/wifi_manager.h"
#include "settings/settings_store.h"
#include "ui/touch_input.h"
#include "ui/ui_controller.h"
#include "weather/openweathermap.h"

#if __has_include("secrets.h")
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
static OpenWeatherMapClient weather(secrets::kOwmApiKey);

static constexpr uint32_t kMsPerMinute = 60000UL;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] CYD weather station starting");

  settings = store.load();
  weather.setLanguage(languageCode(settings.language));

  tft.init();
  touch.begin();
  touch.setCalibration(settings.touch);
  ui.begin(settings);
  wifi.begin(millis());
}

void loop() {
  const uint32_t now = millis();

  wifi.update(now);
  timeSync.update(wifi.isConnected());
  weather.update(now, wifi.isConnected());

  UiState state{};
  state.wifi = wifi.state();
  state.rssi = wifi.rssi();
  state.ssid = wifi.ssid();
  state.ip = wifi.ip();
  state.fetch = weather.status();
  state.dataVersion = weather.version();
  state.data = &weather.data();
  state.dataAgeMin = weather.hasData() ? (now - weather.lastSuccessMs()) / kMsPerMinute : 0;
  state.timeValid = timeSync.localTime(state.localTime);

  const UiCommand cmd = ui.update(state, touch.poll(now), now);
  switch (cmd.action) {
    case UiAction::RefreshWeather:
      weather.requestRefresh();
      break;
    case UiAction::LanguageChanged:
      settings.language = cmd.language;
      weather.setLanguage(languageCode(cmd.language));
      store.save(settings);
      break;
    case UiAction::CalibrationSaved:
      settings.touch = cmd.calibration;
      settings.touchCalibrated = true;
      touch.setCalibration(cmd.calibration);
      store.save(settings);
      break;
    case UiAction::None:
      break;
  }
}
