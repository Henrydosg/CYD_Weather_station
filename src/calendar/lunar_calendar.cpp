#include "lunar_calendar.h"

#include <math.h>

#include "config.h"

namespace {

// Double precision throughout: Julian day numbers are ~2.4 million and the
// new moon must be placed to the minute. The ESP32 computes doubles in
// software, so the dashboard caches results per date.
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kSynodicMonth = 29.530588853;
constexpr double kNewMoonEpoch = 2415021.076998695;  // new moon of 1900-01-01
constexpr int32_t kNewMoonEpochDay = 2415021;
constexpr int32_t kGregorianStart = 2299161;  // 1582-10-15
constexpr int kMaxLunarMonthsInYear = 14;
// Tết always falls between 21 January and 20 February.
constexpr int kTetEarliestDay = 21, kTetLatestDay = 20;
constexpr int kJanuary = 1, kFebruary = 2;

int32_t jdFromDate(int dd, int mm, int yy) {
  const int32_t a = (14 - mm) / 12;
  const int32_t y = yy + 4800 - a;
  const int32_t m = mm + 12 * a - 3;
  int32_t jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  if (jd < kGregorianStart) {
    jd = dd + (153 * m + 2) / 5 + 365 * y + y / 4 - 32083;
  }
  return jd;
}

void jdToDate(int32_t jd, int& dd, int& mm, int& yy) {
  const int32_t a = jd + 32044;
  const int32_t b = (4 * a + 3) / 146097;
  const int32_t c = a - b * 146097 / 4;
  const int32_t d = (4 * c + 3) / 1461;
  const int32_t e = c - 1461 * d / 4;
  const int32_t m = (5 * e + 2) / 153;
  dd = e - (153 * m + 2) / 5 + 1;
  mm = m + 3 - 12 * (m / 10);
  yy = b * 100 + d - 4800 + m / 10;
}

// Julian day of the k-th new moon after 1900-01-01 (Meeus, simplified).
double newMoon(int k) {
  const double t = k / 1236.85;
  const double t2 = t * t, t3 = t2 * t;
  double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 0.000000155 * t3;
  jd1 += 0.00033 * sin((166.56 + 132.87 * t - 0.009173 * t2) * kDegToRad);
  const double m = (359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3) * kDegToRad;
  const double mpr =
      (306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3) * kDegToRad;
  const double f = (21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3) * kDegToRad;
  double c1 = (0.1734 - 0.000393 * t) * sin(m) + 0.0021 * sin(2 * m);
  c1 = c1 - 0.4068 * sin(mpr) + 0.0161 * sin(2 * mpr);
  c1 = c1 - 0.0004 * sin(3 * mpr);
  c1 = c1 + 0.0104 * sin(2 * f) - 0.0051 * sin(m + mpr);
  c1 = c1 - 0.0074 * sin(m - mpr) + 0.0004 * sin(2 * f + m);
  c1 = c1 - 0.0004 * sin(2 * f - m) - 0.0006 * sin(2 * f + mpr);
  c1 = c1 + 0.0010 * sin(2 * f - mpr) + 0.0005 * sin(2 * mpr + m);
  double deltaT;
  if (t < -11) {
    deltaT = 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3;
  } else {
    deltaT = -0.000278 + 0.000265 * t + 0.000262 * t2;
  }
  return jd1 + c1 - deltaT;
}

// Local (UTC+7) day number of the k-th new moon.
int32_t newMoonDay(int k) {
  return static_cast<int32_t>(newMoon(k) + 0.5 + config::kLunarTimeZoneHours / 24.0);
}

// Sun longitude at the start of a local day, as one of 12 sectors (0 = 0..30°).
int sunSector(int32_t dayNumber) {
  const double t = (dayNumber - 2451545.5 - config::kLunarTimeZoneHours / 24.0) / 36525;
  const double t2 = t * t;
  const double m = (357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2) *
                   kDegToRad;
  const double l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2;
  double dl = (1.914600 - 0.004817 * t - 0.000014 * t2) * sin(m);
  dl += (0.019993 - 0.000101 * t) * sin(2 * m) + 0.000290 * sin(3 * m);
  double l = (l0 + dl) * kDegToRad;
  l -= 2 * kPi * floor(l / (2 * kPi));
  return static_cast<int>(l / kPi * 6);
}

// New moon starting lunar month 11 (the one containing the winter solstice) of a year.
int32_t lunarMonth11(int yy) {
  const int32_t off = jdFromDate(31, 12, yy) - kNewMoonEpochDay;
  const int k = static_cast<int>(off / kSynodicMonth);
  int32_t nm = newMoonDay(k);
  if (sunSector(nm) >= 9) {
    nm = newMoonDay(k - 1);
  }
  return nm;
}

// Index (after month 11) of the leap month in a 13-month lunar year.
int leapMonthOffset(int32_t a11) {
  const int k = static_cast<int>((a11 - kNewMoonEpoch) / kSynodicMonth + 0.5);
  int i = 1;
  int arc = sunSector(newMoonDay(k + i));
  int last;
  do {
    last = arc;
    ++i;
    arc = sunSector(newMoonDay(k + i));
  } while (arc != last && i < kMaxLunarMonthsInYear);
  return i - 1;
}

}  // namespace

LunarDate solarToLunar(int day, int month, int year) {
  const int32_t dayNumber = jdFromDate(day, month, year);
  const int k = static_cast<int>((dayNumber - kNewMoonEpoch) / kSynodicMonth);
  int32_t monthStart = newMoonDay(k + 1);
  if (monthStart > dayNumber) {
    monthStart = newMoonDay(k);
  }
  int32_t a11 = lunarMonth11(year);
  int32_t b11 = a11;
  int lunarYear;
  if (a11 >= monthStart) {
    lunarYear = year;
    a11 = lunarMonth11(year - 1);
  } else {
    lunarYear = year + 1;
    b11 = lunarMonth11(year + 1);
  }
  LunarDate out{};
  out.day = static_cast<uint8_t>(dayNumber - monthStart + 1);
  const int diff = static_cast<int>((monthStart - a11) / 29);
  int lunarMonth = diff + 11;
  if (b11 - a11 > 365) {  // 13 lunar months: one of them is a leap month
    const int leapOff = leapMonthOffset(a11);
    if (diff >= leapOff) {
      lunarMonth = diff + 10;
      out.leap = diff == leapOff;
    }
  }
  if (lunarMonth > 12) {
    lunarMonth -= 12;
  }
  if (lunarMonth >= 11 && diff < 4) {
    lunarYear -= 1;
  }
  out.month = static_cast<uint8_t>(lunarMonth);
  out.year = static_cast<uint16_t>(lunarYear);
  return out;
}

int daysToTet(int day, int month, int year) {
  const int32_t today = jdFromDate(day, month, year);
  const int k = static_cast<int>((today - kNewMoonEpoch) / kSynodicMonth);
  // Tết is a new moon in late January / February: test only those.
  for (int i = 0; i <= kMaxLunarMonthsInYear; ++i) {
    const int32_t nm = newMoonDay(k + i);
    if (nm < today) {
      continue;
    }
    int dd, mm, yy;
    jdToDate(nm, dd, mm, yy);
    const bool candidate = (mm == kJanuary && dd >= kTetEarliestDay) ||
                           (mm == kFebruary && dd <= kTetLatestDay);
    if (!candidate) {
      continue;
    }
    const LunarDate l = solarToLunar(dd, mm, yy);
    if (l.day == 1 && l.month == 1 && !l.leap) {
      return static_cast<int>(nm - today);
    }
  }
  return -1;
}
