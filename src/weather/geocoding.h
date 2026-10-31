// City search with the Open-Meteo Geocoding API (no key). The request runs
// on the update() after search(), so the UI can show "Searching..." first.
#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "weather_data.h"

// Name and "admin1, country" region of one Open-Meteo geocoding place, cleaned
// up for the fonts: "Ð" -> "Đ", no "(...)" parts, "Thành phố X" -> "TP. X".
void readPlace(JsonObjectConst place, char* name, size_t nameSize, char* region,
               size_t regionSize);
// Search URL for `query` (percent-encoded); false when it does not fit.
bool buildSearchUrl(char* url, int size, const char* query, uint8_t count, const char* langCode);

class GeocodingClient {
 public:
  // langCode: "vi" or "en" (names come back in that language when known).
  void search(const char* query, const char* langCode);
  void update(bool networkUp);

  SearchStatus status() const { return status_; }
  // Incremented whenever status or results change.
  uint32_t version() const { return version_; }
  uint8_t count() const { return count_; }
  const GeoResult* results() const { return results_; }

 private:
  bool fetch();

  char query_[config::kMaxSearchLen + 1] = "";
  const char* lang_ = "vi";
  bool pending_ = false;
  SearchStatus status_ = SearchStatus::Idle;
  uint32_t version_ = 0;
  uint8_t count_ = 0;
  GeoResult results_[config::kGeoMaxResults];
};
