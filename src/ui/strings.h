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
  LanguageVi,       // "VI" on the language toggle
  LanguageEn,       // "EN"
  Brightness,
  BrightnessAuto,   // Brightness::Auto .. Low, in enum order
  BrightnessHigh,
  BrightnessMedium,
  BrightnessLow,
  BrightnessAutoFmt,  // printf: %u current percent
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
  // Firmware update
  Firmware,
  UpdateVersionFmt,      // printf format, one %s (installed version)
  UpdateTapCheck,
  UpdateChecking,
  UpdateUpToDate,
  UpdateAvailableFmt,    // printf format, one %s (published version)
  UpdateDownloadingFmt,  // printf format, one %u (percent)
  UpdateFailed,
  UpdateRestarting,
  UpdateNoWifi,
  UpdateKeepPower,
  UpdateCheck,
  UpdateInstall,
  // Lunar calendar and rain nowcast (dashboard)
  LunarFmt,          // printf: %u day, %u month, %s leap mark
  LunarLeap,
  TetCountdownFmt,   // printf: %d days
  TetToday,
  FullMoonFmt,       // printf: %u lunar month
  NewMoonFmt,        // printf: %u lunar month
  RainSoonFmt,       // printf: %u minutes
  RainAtFmt,         // printf: %02d hour, %02d minute
  RainStopsFmt,      // printf: %02d hour, %02d minute
  RainContinues,
  // Remote message
  MessageFromFmt,    // printf: %s sender
  MessageTitle,
  MessageDone,
  // Rollback notice (Firmware screen) and device info screen
  UpdateRolledBackFmt,  // printf: %s failed version
  UpdateRolledBackNote,
  InfoButton,
  InfoTitle,
  InfoVersion,
  InfoConfirmedFmt,   // printf: %s version
  InfoVerifyingFmt,   // printf: %s version, %u minutes left
  InfoUptime,
  InfoUptimeDaysFmt,  // printf: %lu days, %02u hours, %02u minutes
  InfoBoots,
  InfoBootsFmt,       // printf: %lu boots, %lu crashes
  InfoLastReset,
  ResetPowerOn,       // ResetKind order
  ResetRestart,
  ResetCrash,
  ResetWatchdog,
  ResetBrownout,
  ResetOther,
  InfoWifi,
  InfoIp,
  InfoMac,
  InfoRam,
  InfoRamFmt,         // printf: %lu KB free, %lu KB lowest
  InfoWeather,
  InfoWeatherAgoFmt,  // printf: %lu minutes
  InfoWeatherNow,
  InfoWeatherNone,
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
