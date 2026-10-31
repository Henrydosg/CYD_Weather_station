// UI text in Vietnamese and English. All on-screen strings go through tr().
#pragma once

#include "../settings/settings.h"

enum class Str : uint8_t {
  LocationName,
  WaitingForData,
  FeelsLike,
  Humidity,
  Wind,
  Pressure,
  NoDataYet,
  JustUpdated,
  MinutesAgoFmt,  // printf format, one %lu
  HoursAgoFmt,    // printf format, one %lu
  TapToRefresh,
  Updating,
  UpdateFailed,
  SyncingTime,
  DateFmt,        // printf format: weekday %s, then two %02d
  Settings,
  Language,
  CalibrateTouch,
  Network,
  NotConnected,
  CalTapTarget,
  CalSaved,
  CalRetry,
  WifiRescan,
  WifiScanning,
  WifiScanFailed,
  WifiNoNetworks,
  WifiConnectedFmt,   // printf format, one %s (network name)
  WifiConnectingFmt,  // printf format, one %s (network name)
  WifiSaved,
  WifiPrev,
  WifiNext,
  KeySpace,
  KeyCancel,
  KeyConnect,
  KeyShow,
  KeyHide,
  WifiConnecting,
  WifiConnectedOk,
  WifiFailedLine1,
  WifiFailedLine2,
  WifiRetry,
  WifiForget,
  Count,
};

void setLanguage(Language lang);
Language currentLanguage();

const char* tr(Str id);
// Weekday name, 0 = Sunday (struct tm::tm_wday).
const char* weekdayName(int wday);
