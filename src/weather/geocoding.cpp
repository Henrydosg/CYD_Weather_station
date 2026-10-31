#include "geocoding.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ctype.h>
#include <string.h>

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

// Drops " (...)" parts, e.g. "Canada (Gia Nã Đại)" -> "Canada".
void stripParentheses(char* s) {
  char* out = s;
  int depth = 0;
  for (const char* p = s; *p != '\0'; ++p) {
    if (*p == '(') {
      ++depth;
    } else if (*p == ')' && depth > 0) {
      --depth;
    } else if (depth == 0) {
      *out++ = *p;
    }
  }
  while (out > s && out[-1] == ' ') {
    --out;
  }
  *out = '\0';
}

// Vietnamese city names: "Thành phố Hồ Chí Minh" -> "TP. Hồ Chí Minh".
void shortenCityPrefix(char* s) {
  static const char* const kPrefixes[] = {"Thành phố ", "Thành Phố "};
  static const char kShort[] = "TP. ";
  for (const char* prefix : kPrefixes) {
    const size_t len = strlen(prefix);
    if (strncmp(s, prefix, len) == 0) {
      const size_t shortLen = strlen(kShort);
      memmove(s + shortLen, s + len, strlen(s + len) + 1);
      memcpy(s, kShort, shortLen);
      return;
    }
  }
}

void cleanPlace(char* s) {
  fixEth(s);
  stripParentheses(s);
}

}  // namespace

void readPlace(JsonObjectConst place, char* name, size_t nameSize, char* region,
               size_t regionSize) {
  char rawName[config::kMaxCityNameLen + 1];
  char admin1[config::kMaxCityRegionLen + 1];
  char country[config::kMaxCityRegionLen + 1];
  copyUtf8(rawName, place["name"] | "", sizeof(rawName));
  copyUtf8(admin1, place["admin1"] | "", sizeof(admin1));
  copyUtf8(country, place["country"] | "", sizeof(country));
  cleanPlace(rawName);
  cleanPlace(admin1);
  cleanPlace(country);

  // Region "admin1, country"; admin1 is left out when it repeats the city.
  char buf[config::kMaxCityRegionLen * 2 + 3];
  if (admin1[0] != '\0' && strcmp(admin1, rawName) != 0) {
    snprintf(buf, sizeof(buf), "%s, %s", admin1, country);
  } else {
    strlcpy(buf, country, sizeof(buf));
  }
  copyUtf8(region, buf, regionSize);
  shortenCityPrefix(rawName);
  copyUtf8(name, rawName, nameSize);
}

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

bool buildSearchUrl(char* url, int size, const char* query, uint8_t count, const char* langCode) {
  int len = snprintf(url, size, "%s?name=", config::kGeocodingUrl);
  appendEncoded(url, len, size, query);
  len += snprintf(url + len, size - len, "&count=%u&language=%s&format=json", count, langCode);
  return len < size;
}

bool GeocodingClient::fetch() {
  char url[kUrlSize];
  if (!buildSearchUrl(url, sizeof(url), query_, config::kGeoMaxResults, lang_)) {
    return false;
  }

  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  for (const char* key :
       {"id", "name", "admin1", "country", "latitude", "longitude", "population"}) {
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
    g.id = r["id"].as<uint32_t>();  // 0 when missing
    readPlace(r, g.name, sizeof(g.name), g.region, sizeof(g.region));
    g.latitude = r["latitude"] | 0.0f;
    g.longitude = r["longitude"] | 0.0f;
    g.population = r["population"] | 0;
  }
  Serial.printf("[geo] \"%s\": %u result(s)\n", query_, count_);
  return true;
}
