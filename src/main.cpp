// CYD (ESP32-2432S028) weather station - entry point.
// Wiring of the modules only; all logic lives in src/net, src/weather, src/ui.
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "net/wifi_manager.h"
#include "ui/screen_main.h"
#include "ui/touch_input.h"
#include "weather/openweathermap.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h - copy include/secrets.example.h and fill in your WiFi credentials"
#endif

static TFT_eSPI tft;
static ScreenMain screen(tft);
static TouchInput touch;
static WifiManager wifi(secrets::kWifiSsid, secrets::kWifiPassword);
static OpenWeatherMapClient weather(secrets::kOwmApiKey);

void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] CYD weather station starting");

  tft.init();
  screen.begin();
  touch.begin();
  wifi.begin(millis());
}

void loop() {
  const uint32_t now = millis();

  wifi.update(now);
  if (touch.tapped(now)) {
    Serial.println("[main] tap -> refresh");
    weather.requestRefresh();
  }
  weather.update(now, wifi.isConnected());

  screen.update(UiState{wifi.state(), weather.status(), weather.version(), &weather.data()});
}
