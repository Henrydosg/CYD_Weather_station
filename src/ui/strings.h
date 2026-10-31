// All on-screen text, Vietnamese and English (UTF-8). Every glyph must exist
// in tools/gen_fonts.py (TEXT_CHARS).
#pragma once

#include <stdint.h>

#include "../settings/settings.h"

enum class Str : uint8_t {
  // Main (dashboard) and graph screens
  WaitingForData,
  FeelsFmt,         // printf format, one %ld (degrees)
  Sunrise,
  Sunset,
  Humidity,
  Wind,
  Pressure,
  Clouds,
  UvIndex,
  Aqi,
  Next24h,
  RainChance,
  HourFmt,          // printf format, one %u (hour)
  // Settings
  Settings,
  Language,
  LanguageName,     // name of the current language
  CalibrateTouch,
  Network,
  NotConnected,
  Cities,
  RotateEvery,
  SecondsFmt,       // printf format, one %u
  // Touch calibration
  CalTapTarget,
  CalSaved,
  CalRetry,
  // WiFi
  WifiRescan,
  WifiScanning,
  WifiScanFailed,
  WifiNoNetworks,
  WifiConnectedFmt,   // printf format, one %s (network name)
  WifiConnectingFmt,  // printf format, one %s (network name)
  WifiSaved,
  WifiPrev,
  WifiNext,
  WifiConnecting,
  WifiConnectedOk,
  WifiFailedLine1,
  WifiFailedLine2,
  WifiRetry,
  WifiForget,
  // Keyboard
  KeySpace,
  KeyCancel,
  KeyConnect,
  KeySearch,
  KeyShow,
  KeyHide,
  // Cities
  AddCity,
  AddCityTitle,
  ResultsFmt,       // printf format, one %s (query)
  Searching,
  SearchFailed,
  NoResults,
  MoveUp,
  MoveDown,
  Remove,
  Close,
  PopMillionsFmt,   // printf format, one %.1f
  PopThousandsFmt,  // printf format, one %.0f
  PopFmt,           // printf format, one %lu
  // Photos (SD card)
  Photos,
  PhotoPrev,
  PhotoNext,
  PhotoBack,
  PhotoCountFmt,    // printf format, %u (count) and %lu (seconds)
  PhotoLoading,
  PhotoNoCard,
  PhotoNoFilesFmt,  // printf format, one %s (folder)
  PhotoError,
  Count,
};

void setLanguage(Language lang);
Language currentLanguage();

const char* tr(Str id);
// Short weekday, 0 = Sunday (struct tm::tm_wday): "CN", "T2" / "Sun", "Mon".
const char* weekdayShort(int wday);
// Description of a WMO weather code ("Mưa nhẹ" / "Light rain").
const char* weatherText(uint8_t wmoCode);
// 8-point compass name for a wind direction in degrees ("ĐN" / "SE").
const char* compassName(uint16_t degrees);
// Level names for the UV index (0 Low .. 4 Extreme) and US AQI (0 Good .. 5 Hazardous).
const char* uvLevelName(uint8_t level);
const char* aqiLevelName(uint8_t level);
