#include "hourglass_app.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "../calendar/lunar_calendar.h"
#include "../ui/day_note.h"
#include "../ui/strings.h"
#include "../ui/weather_icons.h"
#include "config.h"
#include "timefalls_config.h"

namespace tf {

namespace {

constexpr float kMaxAirDt = 0.1f;  // s, after a long frame
constexpr uint32_t kSecondsPerMinute = 60;
constexpr const char* kTaskName = "hourglass";

bool clockValid() { return time(nullptr) >= static_cast<time_t>(config::kMinValidEpoch); }

// Milliseconds to the next :00 of the real minute; false while the clock is
// not set (the minute boundary is the same in UTC and UTC+7).
bool msToNextMinute(uint32_t& out) {
  if (!clockValid()) {
    return false;
  }
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  const uint32_t inMinute = static_cast<uint32_t>(tv.tv_sec % kSecondsPerMinute) * 1000u +
                            static_cast<uint32_t>(tv.tv_usec / 1000);
  out = kSecondsPerMinute * 1000u - inMinute;
  return true;
}

}  // namespace

// --- Public interface (main loop, core 1) -------------------------------------

bool HourglassApp::begin(bool active) {
  lock_ = xSemaphoreCreateMutex();
  const bool geometry = sim_.begin();
  const bool painterOk = geometry && painter_.begin(sim_);
  const bool displayOk = painterOk && display_.begin();
  const bool faceOk = face_.begin();
  if (lock_ == nullptr || !displayOk) {
    Serial.printf("[tf] init failed: geometry %d painter %d display %d, heap %u\n", geometry,
                  painterOk, displayOk, ESP.getFreeHeap());
    return false;
  }
  if (!faceOk) {
    Serial.println("[tf] a text sprite could not be allocated");
  }
  air_.begin(kSeed);
  display_.setAirMask(&ClockFace::covers);  // no motes over the columns' text
  prepareStart();
  wantActive_ = active;
  released_ = !active;
  Serial.printf("[tf] static layer %u bytes, %u colours; heap %u free, largest block %u\n",
                static_cast<unsigned>(painter_.staticBytes()), painter_.staticColours(),
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  return xTaskCreatePinnedToCore(taskEntry, kTaskName, kTaskStackBytes, this, kTaskPriority,
                                 nullptr, kTaskCore) == pdPASS;
}

void HourglassApp::setActive(bool active) {
  if (active) {
    released_ = false;  // the main loop has stopped drawing before calling this
  }
  wantActive_ = active;
}

void HourglassApp::tap(int16_t x, int16_t y) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  tapX_ = x;
  tapY_ = y;
  xSemaphoreGive(lock_);
}

bool HourglassApp::takeWeatherRequest() {
  const bool r = weatherRequest_;
  weatherRequest_ = false;
  return r;
}

bool HourglassApp::takeSettingsRequest() {
  const bool r = settingsRequest_;
  settingsRequest_ = false;
  return r;
}

void HourglassApp::setWeather(const ClockWeather& w) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  shared_ = w;
  ++sharedVersion_;
  xSemaphoreGive(lock_);
}

// --- The task (core 0) -----------------------------------------------------------

void HourglassApp::taskEntry(void* self) { static_cast<HourglassApp*>(self)->run(); }

void HourglassApp::run() {
  esp_task_wdt_add(nullptr);  // the same 30 s watchdog as the main loop
  for (;;) {
    esp_task_wdt_reset();
    iterate(millis());
    vTaskDelay(1);  // let the idle task (and its watchdog) run
  }
}

void HourglassApp::takeDisplay() {
  tft_.setRotation(config::kScreenRotation);
  tft_.setSwapBytes(true);  // our RGB565 buffers; the weather UI expects false
  active_ = true;
  // The hourglass region is right of the text column; while the boot pre-run
  // runs, the column shows the splash instead.
  if (state_ == State::Running) {
    display_.drawAll(sim_);
    face_.redrawAll();
  } else {
    tft_.fillScreen(painter_.background());
    face_.drawSplash();
  }
  lastColumn_ = 0;
  lastFrame_ = millis();
}

void HourglassApp::iterate(uint32_t now) {
  // Display ownership.
  if (wantActive_ && !active_) {
    takeDisplay();
  } else if (!wantActive_ && active_) {
    active_ = false;
    tft_.setSwapBytes(false);
    released_ = true;
  }

  if (state_ == State::Preparing) {
    if (prepareContinue()) {
      startRunning(millis());
    }
    return;
  }

  int16_t tx = -1, ty = -1;
  xSemaphoreTake(lock_, portMAX_DELAY);
  tx = tapX_;
  ty = tapY_;
  tapX_ = tapY_ = -1;
  if (sharedVersion_ != localVersion_) {
    local_ = shared_;
    localVersion_ = sharedVersion_;
  }
  xSemaphoreGive(lock_);
  if (tx >= 0 && active_) {
    handleTap(tx, ty);
  }

  const CycleTick tick = cycle_.update(now);
  if (tick.flipFinished) {
    struct tm t;
    const time_t utc = time(nullptr);
    localtime_r(&utc, &t);
    sim_.setHour(clockValid() ? t.tm_hour : -1);  // for the Hour scene
    sim_.flip();
    sim_.startFlow(cycle_.releaseStartStep(), cycle_.releaseEndStep());
    Serial.printf("[scene] %s\n", sceneName(sim_.scene()));
    if (active_) {
      display_.endRotation(sim_);
    }
    drainSeen_ = false;
    lastFrame_ = now;
  }
  if (tick.flipStarted) {
    reportCycleEnd(tick.boundary);
    alignCycle(now);
    drainSeen_ = false;
    display_.beginRotation(sim_);
    air_.gust(airTime_);  // turning the glass stirs the air
  }

  if (tick.steps > 0) {
    const uint32_t t0 = micros();
    for (uint32_t i = 0; i < tick.steps; ++i) {
      sim_.step();
    }
    stats_.simUs += micros() - t0;
    stats_.simSteps += tick.steps;
    if (!drainSeen_ && sim_.drained()) {
      drainSeen_ = true;
      drainMs_ = cycle_.elapsedMs(now);
    }
  }

  if (active_) {
    const bool rotating = cycle_.phase() == CyclePhase::Rotating;
    if (!cycle_.paused() && (rotating || now - lastFrame_ >= kFrameMs)) {
      const uint32_t t0 = micros();
      if (rotating) {
        display_.drawRotation(sim_, cycle_.flipFraction(now));
        stats_.pixels += static_cast<uint32_t>(kSceneW) * kSceneH;
      } else {
        stats_.pixels += display_.drawDirty(sim_);
        float dt = (now - lastAirMs_) / 1000.0f;
        if (dt > kMaxAirDt) dt = kMaxAirDt;
        airTime_ += dt;
        air_.update(airTime_, dt);
        display_.drawAir(air_);
      }
      lastAirMs_ = now;
      stats_.renderUs += micros() - t0;
      ++stats_.frames;
      lastFrame_ = now;
    } else if (cycle_.paused() && now - lastFrame_ >= kFrameMs) {
      stats_.pixels += display_.drawDirty(sim_);  // a palette change while paused
      lastFrame_ = now;
      lastAirMs_ = now;
    }
    updateColumn(now);  // the column is outside the rotation box
  } else {
    lastAirMs_ = now;
  }
  logDiagnostics(now);
}

void HourglassApp::prepareStart() {
  prepRanks_ = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * kGrains));
  if (prepRanks_ == nullptr) {
    Serial.println("[tf] no RAM for the colour pre-run, using fill order");
  } else {
    for (uint16_t i = 0; i < kGrains; ++i) {
      prepRanks_[i] = 0xFFFF;
    }
  }
  sim_.reset(kSeed);
  sim_.trackArrivals(prepRanks_);
  HourglassCycle first;
  first.start(0);
  sim_.startFlow(first.releaseStartStep(), first.releaseEndStep());
  prepSteps_ = 0;
  state_ = State::Preparing;
}

// Cycle 1 is simulated once at boot (deterministic); each grain's colour band
// is its order of arrival, so the real cycle 1 deposits clean layers.
bool HourglassApp::prepareContinue() {
  const uint32_t maxSteps = HourglassCycle::msToSteps(kCycleMs);
  for (uint32_t i = 0; i < kPrepStepsPerLoop; ++i) {
    if (prepRanks_ == nullptr || sim_.drained() || prepSteps_ >= maxSteps) {
      break;
    }
    sim_.step();
    ++prepSteps_;
  }
  if (prepRanks_ != nullptr && !sim_.drained() && prepSteps_ < maxSteps) {
    return false;
  }
  sim_.assignLayersFromRanks(prepRanks_);  // nullptr: fill order
  if (prepRanks_ != nullptr) {
    Serial.printf("[tf] colour pre-run: drained in %u steps (%.2f s)\n", prepSteps_,
                  prepSteps_ / static_cast<float>(kSimHz));
  }
  sim_.trackArrivals(nullptr);
  free(prepRanks_);
  prepRanks_ = nullptr;
  sim_.reset(kSeed);
  return true;
}

// Cycle 1 always lasts 60 s: it must replay the boot pre-run exactly.
void HourglassApp::startRunning(uint32_t now) {
  cycle_.start(now);
  cycleDeadlineMs_ = cycle_.drainDeadlineMs();
  sim_.startFlow(cycle_.releaseStartStep(), cycle_.releaseEndStep());
  if (active_) {
    display_.drawAll(sim_);
    face_.redrawAll();  // replaces the splash
  }
  drainSeen_ = false;
  lastAirMs_ = now;
  state_ = State::Running;
  Serial.printf("[tf] running: %u grains, heap %u free (min %u, largest block %u)\n", kGrains,
                ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

// Called when a cycle starts after a flip: once the time is known, it ends on
// the next :00 (so every flip is a new minute); otherwise it lasts 60 s.
void HourglassApp::alignCycle(uint32_t now) {
  uint32_t toMinute = 0;
  const bool synced = msToNextMinute(toMinute);
  if (synced) {
    cycle_.setCycleLength(HourglassCycle::alignedLength(now - cycle_.cycleStartMs(), toMinute));
  }
  cycleDeadlineMs_ = cycle_.drainDeadlineMs();
  Serial.printf("[cycle] #%lu starts: %.2f s%s\n", static_cast<unsigned long>(cycle_.cycleIndex() + 1),
                cycle_.cycleLengthMs() / 1000.0f, synced ? ", ends on :00" : " (clock not set)");
}

void HourglassApp::reportCycleEnd(bool automatic) {
  const bool ok = drainSeen_ && drainMs_ <= cycleDeadlineMs_;
  if (automatic && !ok) {
    ++missedDeadlines_;
  }
  const bool verified = !kVerifyOnFlip || sim_.verify();
  Serial.printf("[cycle] #%lu %s: %s%.2f s (deadline %.1f s), pile top row %d, total %s\n",
                static_cast<unsigned long>(cycle_.cycleIndex()), automatic ? "end" : "early flip",
                drainSeen_ ? "drained at " : "NOT drained, ", drainSeen_ ? drainMs_ / 1000.0f : 0.0f,
                cycleDeadlineMs_ / 1000.0f, sim_.lowerPileTop(),
                verified ? "conserved" : "CONSERVATION ERROR");
}

// The hourglass flips by itself every minute and never pauses. Taps: the time
// toggles the diagnostics, the gear opens Settings, the weather (or below it)
// opens the weather station, the hourglass itself changes the sand palette.
void HourglassApp::handleTap(int16_t x, int16_t y) {
  if (ClockFace::inTimeArea(x, y)) {
    showDiag_ = !showDiag_;
    lastColumn_ = 0;
  } else if (ClockFace::inSettingsButton(x, y)) {
    settingsRequest_ = true;
  } else if (ClockFace::inWeatherButton(x, y)) {
    weatherRequest_ = true;
  } else if (ClockFace::inHourglass(x, y)) {
    painter_.setPalette(static_cast<uint8_t>(painter_.palette() + 1));
    display_.invalidate();
    Serial.printf("[tf] palette %s\n", ScenePainter::paletteName(painter_.palette()));
  }
}

const char* HourglassApp::phaseName() const {
  if (cycle_.paused()) return "paused";
  return cycle_.phase() == CyclePhase::Rotating ? "flip" : "flow";
}

void HourglassApp::updateColumn(uint32_t now) {
  // Time and blinking colon: every pass (the sprite is pushed only on change).
  const bool valid = clockValid();
  struct tm t;
  char hhmm[8];
  bool colonOn = (now % 1000u) < 500u;
  if (valid) {
    const time_t utc = time(nullptr);
    localtime_r(&utc, &t);
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    colonOn = tv.tv_usec < 500000;  // blinks with the real seconds
  }
  face_.setTime(valid ? hhmm : nullptr, colonOn);
  face_.setProgress(cycle_.phase() == CyclePhase::Flowing
                        ? cycle_.elapsedMs(now) / static_cast<float>(cycle_.cycleLengthMs())
                        : 0.0f);
  if (lastColumn_ != 0 && now - lastColumn_ < kColumnRefreshMs) {
    return;
  }
  face_.flush();
  lastColumn_ = now;

  // Weekday + date (UI language) or the sync state; lunar date + note of the day.
  if (valid) {
    const int key = (t.tm_year * 12 + t.tm_mon) * 32 + t.tm_mday;
    if (key != lunarKey_) {  // double precision maths: once a day
      lunarKey_ = key;
      lunar_ = solarToLunar(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
      tetDays_ = daysToTet(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    }
    char date[48];
    snprintf(date, sizeof(date), "%s  %02d/%02d", weekdayLong(t.tm_wday), t.tm_mday, t.tm_mon + 1);
    face_.setDate(date, false);
    char lunar[24];
    snprintf(lunar, sizeof(lunar), tr(Str::LunarFmt), lunar_.day, lunar_.month,
             lunar_.leap ? tr(Str::LunarLeap) : "");
    char note[48];
    const bool rain = composeDayNote(local_.valid ? &local_.weather : nullptr,
                                     static_cast<uint32_t>(time(nullptr)), lunar_, tetDays_, note,
                                     sizeof(note));
    face_.setNote(lunar, note, rain);
  } else {
    face_.setDate(tr(local_.wifiUp ? Str::ClockSyncing : Str::ClockNoTime), true);
    face_.setNote("", "", false);
  }

  // Diagnostics (tap the time) in place of the weather.
  if (showDiag_) {
    char diag[96];
    const SandCounts c = sim_.counts();
    snprintf(diag, sizeof(diag), "%.0f fps  %.2f ms/step\nheap %uk / largest %uk\n%s  U%u N%u L%u",
             stats_.fps, stats_.simMsPerStep, ESP.getFreeHeap() / 1024, ESP.getMaxAllocHeap() / 1024,
             sceneName(sim_.scene()), c.upper, c.neck, c.lower);
    face_.setDiag(diag);
  } else {
    face_.setDiag(nullptr);
  }

  // Weather: small icon, temperature, description, city; a placeholder while
  // the first fetch is due (nothing without WiFi).
  if (local_.valid) {
    char temp[12];
    snprintf(temp, sizeof(temp), "%.0f°", local_.weather.temperatureC);
    const Icon& icon = weatherIcon(wmoCategory(local_.weather.wmoCode), local_.weather.isDay, true);
    face_.setWeather(&icon, temp, weatherText(local_.weather.wmoCode), local_.city);
  } else {
    face_.setWeather(nullptr, "", local_.wifiUp ? tr(Str::ClockLoadingWeather) : "", "");
  }
}

void HourglassApp::logDiagnostics(uint32_t now) {
  if (kDiagLogMs == 0 || now - lastDiagLog_ < kDiagLogMs) {
    return;
  }
  const uint32_t span = now - stats_.since;
  if (span > 0) {
    stats_.fps = stats_.frames * 1000.0f / span;
  }
  stats_.simMsPerStep = stats_.simSteps ? stats_.simUs / 1000.0f / stats_.simSteps : 0.0f;
  stats_.renderMs = stats_.frames ? stats_.renderUs / 1000.0f / stats_.frames : 0.0f;
  const SandCounts c = sim_.counts();
  Serial.printf(
      "[diag] %s, %.1f fps, render %.2f ms/frame, sim %.3f ms/step (%lu steps/s), heap %u "
      "(min %u, largest %u), cycle #%lu %s %s %.2f s, U%u N%u L%u total %u, moving %u, "
      "dropped %lu, missed %lu\n",
      active_ ? "shown" : "hidden", stats_.fps, stats_.renderMs, stats_.simMsPerStep,
      static_cast<unsigned long>(span ? stats_.simSteps * 1000ULL / span : 0), ESP.getFreeHeap(),
      ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), static_cast<unsigned long>(cycle_.cycleIndex() + 1),
      phaseName(), sceneName(sim_.scene()), cycle_.elapsedMs(now) / 1000.0f, c.upper, c.neck, c.lower,
      c.total, sim_.movingCount(), static_cast<unsigned long>(cycle_.droppedSteps()),
      static_cast<unsigned long>(missedDeadlines_));
  lastDiagLog_ = now;
  stats_.frames = stats_.simSteps = stats_.simUs = stats_.renderUs = stats_.pixels = 0;
  stats_.since = now;
}

}  // namespace tf
