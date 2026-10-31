#include <math.h>

#include "settings.h"

namespace {
constexpr float kSameCityDeg = 0.01f;  // ~1 km: treat as the same place
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
