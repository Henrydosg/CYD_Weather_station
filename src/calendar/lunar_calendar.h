// Vietnamese lunar calendar (âm lịch), computed on the device: astronomical
// new moons and solar terms at UTC+7 (the usual Vietnamese algorithm), so no
// table and no network. Valid for the years 1900..2199.
#pragma once

#include <stdint.h>

struct LunarDate {
  uint8_t day;    // 1..30
  uint8_t month;  // 1..12
  uint16_t year;
  bool leap;      // leap month (tháng nhuận)
};

// Lunar date of a Gregorian date (day 1..31, month 1..12).
LunarDate solarToLunar(int day, int month, int year);

// Days from a Gregorian date to the next Tết (lunar 1/1): 0 on Tết itself.
int daysToTet(int day, int month, int year);
