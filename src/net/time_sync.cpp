#include "time_sync.h"

#include <Arduino.h>

#include "config.h"

void TimeSync::update(bool networkUp) {
  if (networkUp && !wasUp_) {
    // Sets TZ and (re)starts SNTP; returns immediately.
    configTzTime(config::kTimezonePosix, config::kNtpServer1, config::kNtpServer2);
    Serial.println("[time] SNTP started");
  }
  wasUp_ = networkUp;

  if (!reported_ && isValid()) {
    reported_ = true;
    struct tm t;
    localTime(t);
    Serial.printf("[time] synced: %04d-%02d-%02d %02d:%02d:%02d\n", t.tm_year + 1900,
                  t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  }
}

bool TimeSync::isValid() const {
  return time(nullptr) >= static_cast<time_t>(config::kMinValidEpoch);
}

bool TimeSync::localTime(struct tm& out) const {
  if (!isValid()) {
    return false;
  }
  const time_t now = time(nullptr);
  localtime_r(&now, &out);
  return true;
}
