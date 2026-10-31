// Main screen ("dynamic sky" theme): weather card + detail list on the left,
// analog clock on the right, over a gradient that follows the weather.
// Redraws only the regions whose inputs changed; a palette change redraws all.
#pragma once

#include <TFT_eSPI.h>
#include <time.h>

#include "../net/wifi_manager.h"
#include "../weather/weather_data.h"
#include "analog_clock.h"
#include "theme.h"

struct UiState {
  WifiState wifi;
  int8_t rssi;              // dBm, 0 when not connected
  FetchStatus fetch;
  uint32_t dataVersion;     // 0 = no data yet
  const WeatherData* data;  // valid when dataVersion > 0
  uint32_t dataAgeMin;      // minutes since the last successful fetch
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
  void paintSky(int16_t x, int16_t y, int16_t w, int16_t h);
  uint16_t drawCard(int16_t x, int16_t y, int16_t w, int16_t h);
  void drawHeader(WifiState wifi, uint8_t bars);
  void drawWeather(const WeatherData* data);
  void drawDetails(const WeatherData* data);
  void drawFooter(FetchStatus fetch, const WeatherData* data, uint32_t ageMin);
  void drawDate(const struct tm* t);
  void drawFitted(const char* text, int16_t x, int16_t y, int16_t maxWidth);

  TFT_eSPI& tft_;
  AnalogClock clock_;
  const uint8_t* font_ = nullptr;
  const theme::Palette* palette_ = nullptr;
  bool firstFrame_ = true;
  uint8_t lastBars_ = 0;
  UiState last_{};
};
