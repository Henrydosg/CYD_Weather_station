// Fills in saved city names in both UI languages (Open-Meteo Geocoding API).
// A city added from the search has its name in the search language only; the
// other one is fetched by GeoNames id. Cities saved before ids were stored are
// first matched by name and coordinates. At most one request per update().
#pragma once

#include <stdint.h>

#include "../settings/settings.h"

class CityNameResolver {
 public:
  // `allowed`: network up and no other request due in this loop iteration.
  // Returns true when a city in `cities` was changed (save and redraw).
  bool update(uint32_t now, bool allowed, SavedCities& cities);

 private:
  // false on a network / HTTP error (retried later).
  bool matchByName(City& city);
  bool fetchName(City& city, Language lang);
  // Gives up on the missing names: they fall back to the other language.
  static void giveUp(City& city);

  uint32_t lastRequestAt_ = 0;
  uint32_t waitMs_ = 0;  // before the next request
  uint8_t tries_ = 0;  // failed requests for the current city
  float triesLat_ = 0;
  float triesLon_ = 0;
};
