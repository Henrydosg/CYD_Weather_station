#include "net_worker.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <string.h>

#include "net/ota_updater.h"
#include "net/remote_message.h"
#include "weather/city_names.h"
#include "weather/geocoding.h"
#include "weather/open_meteo.h"

namespace {

// Used by the task only (one instance each, as before in main.cpp).
OpenMeteoClient weather;
GeocodingClient geocoder;
CityNameResolver cityNames;
OtaUpdater ota;
MessageClient messages;

}  // namespace

bool NetWorker::begin(const SavedCities& cities) {
  lock_ = xSemaphoreCreateMutex();
  if (lock_ == nullptr) {
    return false;
  }
  cities_ = cities;
  weather.setCities(cities_);
  return xTaskCreatePinnedToCore(taskEntry, "net", config::kNetTaskStackBytes, this,
                                 config::kNetTaskPriority, nullptr,
                                 config::kNetTaskCore) == pdPASS;
}

void NetWorker::setInputs(bool wifiUp, bool scanning, bool clockMode) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  wifiUp_ = wifiUp;
  scanning_ = scanning;
  clockMode_ = clockMode;
  xSemaphoreGive(lock_);
}

void NetWorker::setCities(const SavedCities& cities) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  cmd_.cities = true;
  cmd_.citiesNew = cities;
  xSemaphoreGive(lock_);
}

void NetWorker::search(const char* query, const char* langCode) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  cmd_.search = true;
  strlcpy(cmd_.query, query, sizeof(cmd_.query));
  cmd_.lang = langCode;
  xSemaphoreGive(lock_);
}

void NetWorker::refreshWeather() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  cmd_.refresh = true;
  xSemaphoreGive(lock_);
}

void NetWorker::checkUpdate() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  cmd_.check = true;
  xSemaphoreGive(lock_);
}

void NetWorker::installUpdate() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  cmd_.install = true;
  xSemaphoreGive(lock_);
}

bool NetWorker::holdIfIdle() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (busy_) {
    xSemaphoreGive(lock_);
    return false;
  }
  return true;  // still held: release() gives it back
}

void NetWorker::release(bool scanning) {
  scanning_ = scanning;
  xSemaphoreGive(lock_);
}

void NetWorker::sync(NetSnapshot& out) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  if (out.weatherStamp != shared_.weatherStamp) {
    memcpy(out.weather, shared_.weather, sizeof(out.weather));
    out.weatherCount = shared_.weatherCount;
    out.weatherStamp = shared_.weatherStamp;
  }
  out.weatherVersion = shared_.weatherVersion;
  out.fetch = shared_.fetch;
  out.lastWeatherMs = shared_.lastWeatherMs;
  if (out.searchVersion != shared_.searchVersion) {
    memcpy(out.results, shared_.results, sizeof(out.results));
    out.searchCount = shared_.searchCount;
    out.searchVersion = shared_.searchVersion;
  }
  out.search = shared_.search;
  out.ota = shared_.ota;
  out.otaDownloading = shared_.otaDownloading;
  if (out.message.id != shared_.message.id) {
    out.message = shared_.message;
  }
  if (out.namesVersion != shared_.namesVersion) {
    out.namedCities = shared_.namedCities;
    out.namesVersion = shared_.namesVersion;
  }
  xSemaphoreGive(lock_);
}

void NetWorker::taskEntry(void* self) { static_cast<NetWorker*>(self)->run(); }

void NetWorker::run() {
  esp_task_wdt_add(nullptr);  // the same watchdog as loop() (kLoopWatchdogSec)
  for (;;) {
    esp_task_wdt_reset();
    pass(millis());
    vTaskDelay(pdMS_TO_TICKS(config::kNetTaskIdleMs));  // loop() may start a scan now
  }
}

// One pass over the clients, as loop() used to do it: at most one request per
// client, each bounded (kTlsHandshakeSec + kHttpTimeoutMs + kHttpBodyMaxMs).
void NetWorker::pass(uint32_t now) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  Commands cmd = cmd_;
  cmd_ = Commands{};
  const bool wifiUp = wifiUp_, scanning = scanning_, clockMode = clockMode_;
  busy_ = true;
  xSemaphoreGive(lock_);

  if (cmd.cities) {
    cities_ = cmd.citiesNew;
    weather.setCities(cities_);
  }
  if (cmd.search) geocoder.search(cmd.query, cmd.lang);
  if (cmd.refresh) weather.requestRefresh();
  if (cmd.check) ota.requestCheck();
  if (cmd.install) ota.requestInstall();

  // While a firmware download runs, nothing else uses the network (RAM for TLS).
  const bool netFree = wifiUp && !ota.downloading();
  // Behind the hourglass clock the air quality (not shown there) is fetched less often.
  const bool airQualityDue = !clockMode || !weather.hasAirQuality() ||
                             now - weather.lastAirQualityMs() >= config::kClockAirQualityRefreshMs;
  // Background requests wait for a WiFi scan (the user's search does not).
  weather.update(now, netFree && !scanning, airQualityDue);
  geocoder.update(netFree);
  // Name lookups only when no weather fetch or search runs in this pass.
  const bool otherRequest = weather.status() == FetchStatus::Fetching ||
                            geocoder.status() == SearchStatus::Searching;
  const bool namesChanged =
      cityNames.update(millis(), netFree && !scanning && !otherRequest, cities_);
  ota.update(millis(), wifiUp, otherRequest || scanning);
  messages.update(millis(), netFree, otherRequest || scanning);

  xSemaphoreTake(lock_, portMAX_DELAY);
  if (namesChanged) {
    shared_.namedCities = cities_;
    ++shared_.namesVersion;
  }
  publish(cmd.cities);
  busy_ = false;
  xSemaphoreGive(lock_);
}

// Under lock_.
// citiesChanged: setCities() may have reordered the data without a new version.
void NetWorker::publish(bool citiesChanged) {
  if (citiesChanged || shared_.weatherVersion != weather.version()) {
    memcpy(shared_.weather, weather.cities(), sizeof(shared_.weather));
    shared_.weatherCount = weather.count();
    shared_.weatherVersion = weather.version();
    ++shared_.weatherStamp;
  }
  shared_.fetch = weather.status();
  shared_.lastWeatherMs = weather.lastSuccessMs();
  if (shared_.searchVersion != geocoder.version()) {
    memcpy(shared_.results, geocoder.results(), sizeof(shared_.results));
    shared_.searchCount = geocoder.count();
    shared_.searchVersion = geocoder.version();
  }
  shared_.search = geocoder.status();
  shared_.ota = ota.status();
  shared_.otaDownloading = ota.downloading();
  if (shared_.message.id != messages.message().id) {
    shared_.message = messages.message();
  }
}
