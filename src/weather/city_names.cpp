#include "city_names.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

#include "geocoding.h"
#include "https_json.h"

namespace {

constexpr int kUrlSize = 256;
constexpr float kMatchDeg = 0.1f;  // ~10 km: a search result is the saved city
constexpr uint8_t kMaxTries = 3;   // failed requests before the names fall back

}  // namespace

bool CityNameResolver::update(uint32_t now, bool allowed, SavedCities& cities) {
  if (!allowed || now - lastRequestAt_ < waitMs_) {
    return false;
  }
  City* city = nullptr;
  for (uint8_t i = 0; i < cities.count; ++i) {
    if (cityNeedsNames(cities.items[i])) {
      city = &cities.items[i];
      break;
    }
  }
  if (city == nullptr) {
    return false;
  }
  if (city->latitude != triesLat_ || city->longitude != triesLon_) {
    tries_ = 0;
    triesLat_ = city->latitude;
    triesLon_ = city->longitude;
  }

  bool ok;
  if (city->geoId == kGeoIdUnknown) {
    ok = matchByName(*city);
  } else {
    const Language missing =
        city->name[static_cast<uint8_t>(Language::Vietnamese)][0] == '\0' ? Language::Vietnamese
                                                                          : Language::English;
    ok = fetchName(*city, missing);
  }
  lastRequestAt_ = millis();  // the request took a while
  if (ok) {
    tries_ = 0;
    waitMs_ = config::kCityNameGapMs;
    return true;
  }
  waitMs_ = config::kCityNameRetryMs;
  if (++tries_ >= kMaxTries) {
    giveUp(*city);
    tries_ = 0;
    return true;
  }
  return false;
}

bool CityNameResolver::matchByName(City& city) {
  const uint8_t vi = static_cast<uint8_t>(Language::Vietnamese);
  char url[kUrlSize];
  if (!buildSearchUrl(url, sizeof(url), cityName(city, Language::Vietnamese),
                      config::kGeoMatchResults, languageCode(Language::Vietnamese))) {
    giveUp(city);
    return true;
  }
  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  for (const char* key : {"id", "name", "admin1", "country", "latitude", "longitude"}) {
    f[key] = true;
  }
  JsonDocument doc;
  if (!httpsGetJson(url, filter, doc, "names")) {
    return false;
  }
  for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
    const float lat = r["latitude"] | 0.0f;
    const float lon = r["longitude"] | 0.0f;
    if (fabsf(lat - city.latitude) < kMatchDeg && fabsf(lon - city.longitude) < kMatchDeg) {
      city.geoId = r["id"].as<uint32_t>();
      readPlace(r, city.name[vi], sizeof(city.name[vi]), city.region[vi],
                sizeof(city.region[vi]));
      Serial.printf("[names] matched id %lu\n", static_cast<unsigned long>(city.geoId));
      if (city.geoId == kGeoIdUnknown) {
        giveUp(city);
      }
      return true;
    }
  }
  Serial.println("[names] no match by name, keeping the saved name");
  giveUp(city);
  return true;
}

bool CityNameResolver::fetchName(City& city, Language lang) {
  const uint8_t l = static_cast<uint8_t>(lang);
  char url[kUrlSize];
  snprintf(url, sizeof(url), "%s?id=%lu&language=%s", config::kGeocodingGetUrl,
           static_cast<unsigned long>(city.geoId), languageCode(lang));
  JsonDocument filter;
  for (const char* key : {"name", "admin1", "country"}) {
    filter[key] = true;
  }
  JsonDocument doc;
  if (!httpsGetJson(url, filter, doc, "names")) {
    return false;
  }
  if (!doc["name"].is<const char*>()) {
    giveUp(city);
    return true;
  }
  readPlace(doc.as<JsonObjectConst>(), city.name[l], sizeof(city.name[l]), city.region[l],
            sizeof(city.region[l]));
  Serial.printf("[names] id %lu: %s name\n", static_cast<unsigned long>(city.geoId),
                languageCode(lang));
  return true;
}

void CityNameResolver::giveUp(City& city) {
  for (uint8_t i = 0; i < kLanguageCount; ++i) {
    const uint8_t other = kLanguageCount - 1 - i;
    if (city.name[i][0] == '\0') {
      strlcpy(city.name[i], city.name[other], sizeof(city.name[i]));
      strlcpy(city.region[i], city.region[other], sizeof(city.region[i]));
    }
  }
  if (city.geoId == kGeoIdUnknown) {
    city.geoId = kGeoIdNone;
  }
}
