// Main weather screen. Redraws only the regions whose inputs changed.
#pragma once

#include <TFT_eSPI.h>

#include "../net/wifi_manager.h"
#include "../weather/weather_data.h"

struct UiState {
  WifiState wifi;
  FetchStatus fetch;
  uint32_t dataVersion;     // 0 = no data yet
  const WeatherData* data;  // valid when dataVersion > 0
};

class ScreenMain {
 public:
  explicit ScreenMain(TFT_eSPI& tft) : tft_(tft) {}

  void begin();
  // Cheap to call every loop iteration; draws only on change.
  void update(const UiState& state);

 private:
  void drawHeader(WifiState wifi);
  void drawWeather(const WeatherData* data);
  void drawFooter(FetchStatus fetch, const WeatherData* data);
  void drawIcon(int16_t cx, int16_t cy, WeatherCategory category, bool isDay);
  void drawSun(int16_t cx, int16_t cy, int16_t r);
  void drawMoon(int16_t cx, int16_t cy, int16_t r);
  void drawCloud(int16_t cx, int16_t cy, uint16_t color);

  TFT_eSPI& tft_;
  bool firstFrame_ = true;
  UiState last_{};
};
