// Project-wide configuration for the CYD (ESP32-2432S028) weather station.
// Every tunable constant lives here. TFT pins are set in platformio.ini
// build_flags (TFT_eSPI requires them at compile time).
#pragma once

#include <stdint.h>

namespace config {

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
constexpr uint8_t kScreenRotation = 1;  // landscape, 320 x 240
// Some CYD panel variants (e.g. "TPM408-2.8") show a colour negative unless
// display inversion is enabled. Set to false if colours look inverted.
constexpr bool kInvertDisplay = true;
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// ---------------------------------------------------------------------------
// Touch controller XPT2046 (own SPI bus, VSPI)
// ---------------------------------------------------------------------------
constexpr uint8_t kTouchClk = 25;
constexpr uint8_t kTouchMosi = 32;
constexpr uint8_t kTouchMiso = 39;
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchIrq = 36;
constexpr uint32_t kTouchDebounceMs = 300;
constexpr uint8_t kTouchSamples = 4;  // raw readings averaged per tap
// Default raw->screen mapping (raw value at screen x = 0 / x = 319 and
// y = 0 / y = 239) until the user calibrates. Typical for CYD boards.
constexpr int16_t kTouchRawLeft = 200;
constexpr int16_t kTouchRawRight = 3700;
constexpr int16_t kTouchRawTop = 240;
constexpr int16_t kTouchRawBottom = 3800;
// Calibration: targets are drawn this far from two opposite corners, and a
// result spanning fewer raw units than this is rejected as a mis-tap.
constexpr int16_t kCalTargetInset = 24;
constexpr int16_t kCalMinRawSpan = 1000;
constexpr uint32_t kCalDoneMessageMs = 1500;

// ---------------------------------------------------------------------------
// Settings (stored in NVS via Preferences)
// ---------------------------------------------------------------------------
constexpr char kSettingsNamespace[] = "cyd-weather";
constexpr uint8_t kDefaultLanguage = 0;  // 0 = Vietnamese, 1 = English

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
constexpr char kHostname[] = "cyd-weather";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiRetryInitialMs = 5000;
constexpr uint32_t kWifiRetryMaxMs = 60000;
constexpr uint32_t kRssiSampleMs = 5000;  // signal strength refresh for the UI

// ---------------------------------------------------------------------------
// Time (SNTP)
// ---------------------------------------------------------------------------
// POSIX TZ string: "<+07>-7" = UTC+7, no DST (Vietnam).
constexpr char kTimezonePosix[] = "<+07>-7";
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.google.com";
// Any epoch before this means the clock has not been set yet (2024-01-01).
constexpr uint32_t kMinValidEpoch = 1704067200UL;

// ---------------------------------------------------------------------------
// Location / OpenWeatherMap (Current Weather API 2.5, free tier)
// ---------------------------------------------------------------------------
// UTF-8; every glyph must exist in tools/gen_fonts.py.
constexpr char kLocationNameVi[] = "TP. Hồ Chí Minh";
constexpr char kLocationNameEn[] = "Ho Chi Minh City";
constexpr char kLatitude[] = "10.82";
constexpr char kLongitude[] = "106.63";

constexpr char kOwmBaseUrl[] = "https://api.openweathermap.org/data/2.5/weather";
constexpr char kOwmUnits[] = "metric";  // temperature in C, wind in m/s
constexpr uint32_t kWeatherRefreshMs = 10UL * 60UL * 1000UL;  // 10 minutes (limit: 60/min)
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;           // after an error
constexpr uint16_t kHttpTimeoutMs = 5000;

}  // namespace config
