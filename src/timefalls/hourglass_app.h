// TIME FALLS inside the weather station: the hourglass clock screen as its own
// FreeRTOS task on core 0, so the sand keeps falling while the main loop
// (core 1) blocks in an HTTPS request.
//
// The display belongs to one side at a time. setActive(true) gives it to the
// clock, which repaints the whole screen; setActive(false) asks the clock to
// let go: once displayReleased() is true the weather UI may draw. While
// inactive the simulation and the 60 s cycle keep running without drawing,
// so returning to the clock shows the right moment of the minute.
//
// Data only crosses through plain structs under a mutex: the weather
// snapshot (main -> clock), taps (main -> clock), the request to open the
// weather station (clock -> main).
#pragma once

#include <TFT_eSPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#include "../calendar/lunar_calendar.h"
#include "../weather/weather_data.h"
#include "air_field.h"
#include "clock_face.h"
#include "hourglass_cycle.h"
#include "sand_sim.h"
#include "scene_display.h"
#include "scene_painter.h"

namespace tf {

// What the clock shows of the weather (copied from the main loop).
struct ClockWeather {
  bool valid = false;      // a forecast for the city exists
  CityWeather weather;     // current conditions, nowcast
  char city[48] = "";      // name in the UI language
  bool wifiUp = false;
  bool fetching = false;   // first fetch running (no data yet)
};

class HourglassApp {
 public:
  explicit HourglassApp(TFT_eSPI& tft)
      : tft_(tft), display_(tft, painter_), face_(tft) {}

  // Allocates the buffers and starts the task. active: draws at once (splash
  // and boot pre-run), else runs without the display. False if out of memory.
  bool begin(bool active);
  void setActive(bool active);
  bool displayReleased() const { return released_; }
  bool active() const { return wantActive_; }

  // Called by the main loop (which polls the touch panel) in clock mode.
  void tap(int16_t x, int16_t y);
  // True once after the user tapped the weather block.
  bool takeWeatherRequest();
  // True once after the user tapped the Settings gear.
  bool takeSettingsRequest();
  void setWeather(const ClockWeather& w);

 private:
  enum class State : uint8_t { Preparing, Running };
  struct Stats {
    uint32_t frames = 0, simSteps = 0, simUs = 0, renderUs = 0, pixels = 0, since = 0;
    float fps = 0, simMsPerStep = 0, renderMs = 0;
  };

  static void taskEntry(void* self);
  void run();
  void iterate(uint32_t now);
  void takeDisplay();
  void prepareStart();
  bool prepareContinue();
  void startRunning(uint32_t now);
  void alignCycle(uint32_t now);
  void reportCycleEnd(bool automatic);
  void handleTap(int16_t x, int16_t y);
  void updateColumn(uint32_t now);
  void logDiagnostics(uint32_t now);
  const char* phaseName() const;

  TFT_eSPI& tft_;
  SandSim sim_;
  ScenePainter painter_;
  SceneDisplay display_;
  ClockFace face_;
  HourglassCycle cycle_;
  AirField air_;

  // Shared with the main loop
  SemaphoreHandle_t lock_ = nullptr;
  ClockWeather shared_;
  uint32_t sharedVersion_ = 0;
  int16_t tapX_ = -1, tapY_ = -1;
  volatile bool wantActive_ = false;
  volatile bool released_ = true;
  volatile bool weatherRequest_ = false;
  volatile bool settingsRequest_ = false;

  // Task-local
  ClockWeather local_;
  uint32_t localVersion_ = 0;
  bool active_ = false;
  State state_ = State::Preparing;
  uint16_t* prepRanks_ = nullptr;
  uint32_t prepSteps_ = 0;
  bool showDiag_ = false;
  float airTime_ = 0;
  uint32_t lastAirMs_ = 0, lastFrame_ = 0, lastColumn_ = 0, lastDiagLog_ = 0;
  bool drainSeen_ = false;
  uint32_t drainMs_ = 0;
  uint32_t cycleDeadlineMs_ = kCycleMs - kDrainMarginMs;
  uint32_t missedDeadlines_ = 0;
  int lunarKey_ = -1;
  LunarDate lunar_{};
  int tetDays_ = -1;
  Stats stats_;
};

}  // namespace tf
