#include "open_meteo.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

#include "https_json.h"

namespace {

constexpr int kUrlSize = 1024;
constexpr int32_t kSecondsPerDay = 86400;
constexpr int32_t kSecondsPerMinute = 60;
constexpr float kSameCityDeg = 0.01f;

constexpr char kForecastParams[] =
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day,"
    "wind_speed_10m,wind_direction_10m,surface_pressure,cloud_cover,uv_index"
    "&hourly=temperature_2m,weather_code,is_day,precipitation_probability"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
    "&timezone=auto&timeformat=unixtime&forecast_days=%u&forecast_hours=%u";
constexpr char kAirQualityParams[] = "&current=us_aqi";

// Broken-down local time of a UTC unix time in a city.
struct tm cityTime(int64_t utc, int32_t offsetSec) {
  const time_t t = static_cast<time_t>(utc + offsetSec);
  struct tm out;
  gmtime_r(&t, &out);
  return out;
}

int16_t minuteOfDay(int64_t utc, int32_t offsetSec) {
  int64_t sec = (utc + offsetSec) % kSecondsPerDay;
  if (sec < 0) {
    sec += kSecondsPerDay;
  }
  return static_cast<int16_t>(sec / kSecondsPerMinute);
}

void parseCity(JsonObjectConst o, CityWeather& w) {
  w.utcOffsetSec = o["utc_offset_seconds"] | 0;

  JsonObjectConst c = o["current"];
  w.temperatureC = c["temperature_2m"] | 0.0f;
  w.apparentC = c["apparent_temperature"] | 0.0f;
  w.humidityPct = c["relative_humidity_2m"] | 0.0f;
  w.wmoCode = c["weather_code"] | 0;
  w.isDay = (c["is_day"] | 1) != 0;
  w.windKmh = c["wind_speed_10m"] | 0.0f;
  w.windDirDeg = c["wind_direction_10m"] | 0;
  w.pressureHpa = c["surface_pressure"] | 0.0f;
  w.cloudPct = c["cloud_cover"] | 0;
  w.uvIndex = c["uv_index"] | 0.0f;

  JsonObjectConst d = o["daily"];
  JsonArrayConst days = d["time"];
  w.dailyCount = 0;
  for (size_t i = 0; i < days.size() && w.dailyCount < config::kForecastDays; ++i) {
    const struct tm t = cityTime(days[i].as<int64_t>(), w.utcOffsetSec);
    DailyForecast& f = w.daily[w.dailyCount++];
    f.mday = t.tm_mday;
    f.month = t.tm_mon + 1;
    f.wday = t.tm_wday;
    f.wmoCode = d["weather_code"][i] | 0;
    f.maxC = d["temperature_2m_max"][i] | 0.0f;
    f.minC = d["temperature_2m_min"][i] | 0.0f;
  }
  w.sunriseMin = d["sunrise"][0].is<int64_t>()
                     ? minuteOfDay(d["sunrise"][0].as<int64_t>(), w.utcOffsetSec)
                     : -1;
  w.sunsetMin = d["sunset"][0].is<int64_t>()
                    ? minuteOfDay(d["sunset"][0].as<int64_t>(), w.utcOffsetSec)
                    : -1;

  JsonObjectConst h = o["hourly"];
  JsonArrayConst hours = h["time"];
  w.hourlyCount = 0;
  constexpr uint8_t kPoints = config::kForecastHours / config::kHourlyStep;
  for (size_t i = 0; i < hours.size() && w.hourlyCount < kPoints; i += config::kHourlyStep) {
    HourlyPoint& p = w.hourly[w.hourlyCount++];
    p.hour = cityTime(hours[i].as<int64_t>(), w.utcOffsetSec).tm_hour;
    p.tempC = h["temperature_2m"][i] | 0.0f;
    p.wmoCode = h["weather_code"][i] | 0;
    p.isDay = (h["is_day"][i] | 1) != 0;
    p.precipPct = h["precipitation_probability"][i] | 0;
  }
  w.valid = true;
}

}  // namespace

void OpenMeteoClient::setCities(const SavedCities& cities) {
  CityWeather old[config::kMaxCities];
  for (int i = 0; i < count_; ++i) {
    old[i] = data_[i];
  }
  const uint8_t oldCount = count_;
  count_ = cities.count;
  bool missing = false;
  for (int i = 0; i < count_; ++i) {
    const City& c = cities.items[i];
    data_[i] = CityWeather{};
    for (int j = 0; j < oldCount; ++j) {
      if (fabsf(old[j].latitude - c.latitude) < kSameCityDeg &&
          fabsf(old[j].longitude - c.longitude) < kSameCityDeg) {
        data_[i] = old[j];
        break;
      }
    }
    data_[i].latitude = c.latitude;
    data_[i].longitude = c.longitude;
    missing = missing || !data_[i].valid;
  }
  ++version_;
  if (missing) {
    requestRefresh();
  }
}

int OpenMeteoClient::appendCoordinates(char* buf, int len, int size) const {
  const char* keys[] = {"?latitude=", "&longitude="};
  for (int k = 0; k < 2 && len < size; ++k) {
    len += snprintf(buf + len, size - len, "%s", keys[k]);
    for (int i = 0; i < count_ && len < size; ++i) {
      const float v = k == 0 ? data_[i].latitude : data_[i].longitude;
      len += snprintf(buf + len, size - len, i == 0 ? "%.4f" : ",%.4f", v);
    }
  }
  return len;
}

bool OpenMeteoClient::fetchForecast() {
  char url[kUrlSize];
  int len = snprintf(url, sizeof(url), "%s", config::kForecastUrl);
  len = appendCoordinates(url, len, sizeof(url));
  len += snprintf(url + len, sizeof(url) - len, kForecastParams, config::kForecastDays,
                  config::kForecastHours);
  if (len >= static_cast<int>(sizeof(url))) {
    Serial.println("[weather] URL too long");
    return false;
  }

  // One location answers with an object, several with an array of objects.
  JsonDocument one;
  one["utc_offset_seconds"] = true;
  one["current"] = true;
  one["daily"] = true;
  one["hourly"] = true;
  JsonDocument filter;
  if (count_ > 1) {
    filter.add(one);
  } else {
    filter.set(one);
  }

  Serial.printf("[weather] GET forecast for %u city(ies)\n", count_);
  JsonDocument doc;
  if (!httpsGetJson(url, filter, doc, "weather")) {
    return false;
  }
  if (count_ > 1) {
    JsonArrayConst arr = doc.as<JsonArrayConst>();
    if (arr.size() != count_) {
      Serial.printf("[weather] expected %u locations, got %u\n", count_, arr.size());
      return false;
    }
    for (int i = 0; i < count_; ++i) {
      parseCity(arr[i], data_[i]);
    }
  } else {
    parseCity(doc.as<JsonObjectConst>(), data_[0]);
  }
  const CityWeather& w = data_[0];
  Serial.printf("[weather] ok, first city %.1f C, code %u, %u days, %u hourly points\n",
                w.temperatureC, w.wmoCode, w.dailyCount, w.hourlyCount);
  return true;
}

bool OpenMeteoClient::fetchAirQuality() {
  char url[kUrlSize];
  int len = snprintf(url, sizeof(url), "%s", config::kAirQualityUrl);
  len = appendCoordinates(url, len, sizeof(url));
  len += snprintf(url + len, sizeof(url) - len, "%s", kAirQualityParams);
  if (len >= static_cast<int>(sizeof(url))) {
    return false;
  }
  JsonDocument one;
  one["current"]["us_aqi"] = true;
  JsonDocument filter;
  if (count_ > 1) {
    filter.add(one);
  } else {
    filter.set(one);
  }
  JsonDocument doc;
  if (!httpsGetJson(url, filter, doc, "aqi")) {
    return false;
  }
  for (int i = 0; i < count_; ++i) {
    JsonVariantConst location =
        count_ > 1 ? doc[i].as<JsonVariantConst>() : doc.as<JsonVariantConst>();
    JsonVariantConst aqi = location["current"]["us_aqi"];
    data_[i].usAqi = aqi.is<int>() ? aqi.as<int>() : -1;
  }
  Serial.printf("[aqi] ok, first city %d\n", data_[0].usAqi);
  return true;
}

void OpenMeteoClient::update(uint32_t now, bool networkUp) {
  if (!networkUp || count_ == 0) {
    return;
  }

  // Second phase: the UI has had one loop iteration to show "Fetching".
  if (status_ == FetchStatus::Fetching) {
    if (step_ == Step::Forecast) {
      if (fetchForecast()) {
        ++version_;
        lastSuccess_ = millis();
        step_ = Step::AirQuality;  // next loop iteration
        return;
      }
      status_ = FetchStatus::Error;
      nextDelayMs_ = config::kWeatherRetryMs;
    } else {
      if (fetchAirQuality()) {  // optional: the forecast is shown without it
        ++version_;
      }
      status_ = FetchStatus::Ok;
      nextDelayMs_ = config::kWeatherRefreshMs;
      step_ = Step::Forecast;
    }
    lastAttempt_ = millis();  // the request took time; measure from its end
    return;
  }

  const bool due = (now - lastAttempt_) >= nextDelayMs_;
  if (refreshRequested_ || due) {
    refreshRequested_ = false;
    status_ = FetchStatus::Fetching;
    step_ = Step::Forecast;
  }
}
