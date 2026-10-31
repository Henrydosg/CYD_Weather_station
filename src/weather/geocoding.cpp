#include "geocoding.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ctype.h>

#include "https_json.h"

namespace {

constexpr int kUrlSize = 256;

// Percent-encodes everything except letters and digits.
void appendEncoded(char* buf, int& len, int size, const char* text) {
  static const char kHex[] = "0123456789ABCDEF";
  for (const char* p = text; *p != '\0' && len + 3 < size; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (isalnum(c)) {
      buf[len++] = static_cast<char>(c);
    } else {
      buf[len++] = '%';
      buf[len++] = kHex[c >> 4];
      buf[len++] = kHex[c & 0x0F];
    }
  }
  buf[len] = '\0';
}

// The geocoder sometimes writes Vietnamese "Đ/đ" as Icelandic "Ð/ð"
// (U+00D0/U+00F0), which the fonts do not contain. Same byte length.
void fixEth(char* s) {
  for (unsigned char* p = reinterpret_cast<unsigned char*>(s); p[0] != 0 && p[1] != 0; ++p) {
    if (p[0] == 0xC3 && (p[1] == 0x90 || p[1] == 0xB0)) {
      p[0] = 0xC4;
      p[1] = p[1] == 0x90 ? 0x90 : 0x91;
    }
  }
}

}  // namespace

void GeocodingClient::search(const char* query, const char* langCode) {
  strlcpy(query_, query, sizeof(query_));
  lang_ = langCode;
  pending_ = true;
  status_ = SearchStatus::Searching;
  count_ = 0;
  ++version_;
}

void GeocodingClient::update(bool networkUp) {
  if (!pending_) {
    return;
  }
  pending_ = false;
  status_ = networkUp && fetch() ? SearchStatus::Done : SearchStatus::Failed;
  ++version_;
}

bool GeocodingClient::fetch() {
  char url[kUrlSize];
  int len = snprintf(url, sizeof(url), "%s?name=", config::kGeocodingUrl);
  appendEncoded(url, len, sizeof(url), query_);
  len += snprintf(url + len, sizeof(url) - len, "&count=%u&language=%s&format=json",
                  config::kGeoMaxResults, lang_);
  if (len >= kUrlSize) {
    return false;
  }

  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  for (const char* key : {"name", "admin1", "country", "latitude", "longitude", "population"}) {
    f[key] = true;
  }
  JsonDocument doc;
  if (!httpsGetJson(url, filter, doc, "geo")) {
    return false;
  }

  // No "results" key at all means nothing was found (not an error).
  for (JsonObjectConst r : doc["results"].as<JsonArrayConst>()) {
    if (count_ >= config::kGeoMaxResults) {
      break;
    }
    GeoResult& g = results_[count_++];
    copyUtf8(g.name, r["name"] | "", sizeof(g.name));
    const char* admin1 = r["admin1"] | "";
    const char* country = r["country"] | "";
    char region[96];
    snprintf(region, sizeof(region), admin1[0] != '\0' ? "%s, %s" : "%s%s", admin1, country);
    copyUtf8(g.region, region, sizeof(g.region));
    fixEth(g.name);
    fixEth(g.region);
    g.latitude = r["latitude"] | 0.0f;
    g.longitude = r["longitude"] | 0.0f;
    g.population = r["population"] | 0;
  }
  Serial.printf("[geo] \"%s\": %u result(s)\n", query_, count_);
  return true;
}
