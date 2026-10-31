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
constexpr uint32_t kTouchDebounceMs = 600;

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
constexpr char kHostname[] = "cyd-weather";
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiRetryInitialMs = 5000;
constexpr uint32_t kWifiRetryMaxMs = 60000;

// ---------------------------------------------------------------------------
// Location / Open-Meteo
// ---------------------------------------------------------------------------
constexpr char kLocationName[] = "Ho Chi Minh City";
constexpr char kLatitude[] = "10.82";
constexpr char kLongitude[] = "106.63";
constexpr char kTimezone[] = "auto";

constexpr char kOpenMeteoHost[] = "http://api.open-meteo.com";
constexpr uint32_t kWeatherRefreshMs = 10UL * 60UL * 1000UL;  // 10 minutes
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;           // after an error
constexpr uint16_t kHttpTimeoutMs = 5000;

}  // namespace config
