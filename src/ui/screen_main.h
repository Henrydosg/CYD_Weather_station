// Main screen: weather card + detail list on the left, analog clock on the
// right. Redraws only the regions whose inputs changed.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

#include "../net/wifi_manager.h"
#include "../weather/weather_data.h"
#include "analog_clock.h"

struct UiState {
  WifiState wifi;
  FetchStatus fetch;
  uint32_t dataVersion;     // 0 = no data yet
  const WeatherData* data;  // valid when dataVersion > 0
  bool timeValid;
  struct tm localTime;      // valid when timeValid
};

class ScreenMain {
 public:
  explicit ScreenMain(TFT_eSPI& tft);

  void begin();
  // Cheap to call every loop iteration; draws only on change.
  void update(const UiState& state);

 private:
  void useFont(const uint8_t* font);
  void drawHeader(WifiState wifi);
  void drawWeather(const WeatherData* data);
  void drawDetails(const WeatherData* data);
  void drawFooter(FetchStatus fetch, const WeatherData* data);
  void drawDate(const struct tm* t);
  void drawFitted(const char* text, int16_t x, int16_t y, int16_t maxWidth);

  TFT_eSPI& tft_;
  AnalogClock clock_;
  const uint8_t* font_ = nullptr;
  bool firstFrame_ = true;
  UiState last_{};
};
