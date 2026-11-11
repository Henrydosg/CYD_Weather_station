#include "day_note.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "strings.h"

namespace {

constexpr uint32_t kSecondsPerMinute = 60;
constexpr uint32_t kMinutesPerHour = 60;
constexpr uint32_t kRoundMinutes = 5;  // "~25 min"
constexpr int kFullMoonDay = 15;

// Local wall-clock time of a UTC instant in a city.
struct tm localTime(uint32_t utc, int32_t offsetSec) {
  const time_t t = static_cast<time_t>(utc) + offsetSec;
  struct tm out;
  gmtime_r(&t, &out);
  return out;
}

bool rainNote(const CityWeather& w, uint32_t now, char* out, size_t size) {
  const RainOutlook outlook = rainOutlook(w, now);
  const struct tm at = localTime(outlook.atUtc, w.utcOffsetSec);
  switch (outlook.kind) {
    case RainKind::Soon: {
      const uint32_t minutes = (outlook.atUtc - now + kSecondsPerMinute - 1) / kSecondsPerMinute;
      if (minutes < kMinutesPerHour) {
        uint32_t rounded = (minutes + kRoundMinutes - 1) / kRoundMinutes * kRoundMinutes;
        if (rounded < kRoundMinutes) {
          rounded = kRoundMinutes;
        }
        snprintf(out, size, tr(Str::RainSoonFmt), static_cast<unsigned>(rounded));
      } else {
        snprintf(out, size, tr(Str::RainAtFmt), at.tm_hour, at.tm_min);
      }
      return true;
    }
    case RainKind::Raining:
      snprintf(out, size, tr(Str::RainStopsFmt), at.tm_hour, at.tm_min);
      return true;
    case RainKind::Continues:
      strlcpy(out, tr(Str::RainContinues), size);
      return true;
    case RainKind::None:
      break;
  }
  return false;
}

}  // namespace

bool composeDayNote(const CityWeather* w, uint32_t utcNow, const LunarDate& lunar, int tetDays,
                    char* out, size_t size) {
  if (size == 0) {
    return false;
  }
  out[0] = '\0';
  if (w != nullptr && w->valid && rainNote(*w, utcNow, out, size)) {
    return true;
  }
  if (lunar.day == 1 && lunar.month == 1 && !lunar.leap) {
    strlcpy(out, tr(Str::TetToday), size);
  } else if (lunar.day == kFullMoonDay) {
    snprintf(out, size, tr(Str::FullMoonFmt), lunar.month);
  } else if (lunar.day == 1) {
    snprintf(out, size, tr(Str::NewMoonFmt), lunar.month);
  } else if (tetDays > 0) {
    snprintf(out, size, tr(Str::TetCountdownFmt), tetDays);
  }
  return false;
}
