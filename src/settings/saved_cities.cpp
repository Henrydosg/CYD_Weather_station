#include <math.h>

#include "settings.h"

namespace {

constexpr float kSameCityDeg = 0.01f;  // ~1 km: treat as the same place

const char* pick(const char* wanted, const char* other) {
  return wanted[0] != '\0' ? wanted : other;
}

}  // namespace

const char* cityName(const City& city, Language lang) {
  const uint8_t i = static_cast<uint8_t>(lang);
  return pick(city.name[i], city.name[1 - i]);
}

const char* cityRegion(const City& city, Language lang) {
  const uint8_t i = static_cast<uint8_t>(lang);
  return pick(city.region[i], city.region[1 - i]);
}

bool cityNeedsNames(const City& city) {
  if (city.geoId == kGeoIdNone) {
    return false;
  }
  for (uint8_t i = 0; i < kLanguageCount; ++i) {
    if (city.name[i][0] == '\0') {
      return true;
    }
  }
  return city.geoId == kGeoIdUnknown;
}

bool addCity(SavedCities& cities, const City& city) {
  if (cities.count >= config::kMaxCities) {
    return false;
  }
  for (int i = 0; i < cities.count; ++i) {
    if (fabsf(cities.items[i].latitude - city.latitude) < kSameCityDeg &&
        fabsf(cities.items[i].longitude - city.longitude) < kSameCityDeg) {
      return false;
    }
  }
  cities.items[cities.count++] = city;
  return true;
}

bool moveCity(SavedCities& cities, uint8_t index, int8_t delta) {
  const int other = index + delta;
  if (index >= cities.count || other < 0 || other >= cities.count) {
    return false;
  }
  const City tmp = cities.items[index];
  cities.items[index] = cities.items[other];
  cities.items[other] = tmp;
  return true;
}

bool removeCity(SavedCities& cities, uint8_t index) {
  if (index >= cities.count || cities.count <= 1) {
    return false;  // keep at least one city
  }
  for (int i = index; i + 1 < cities.count; ++i) {
    cities.items[i] = cities.items[i + 1];
  }
  --cities.count;
  return true;
}
