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
// Gamma after init. The ILI9341_2_DRIVER tables end the positive curve with
// zeros, so bright tones come out dark and photos look like a negative (flat
// UI colours hide it). Standard = the ILI9341 datasheet / ILI9341_DRIVER tables.
enum class PanelGamma : uint8_t {
  Driver,    // keep the init tables
  Reselect,  // GAMMASET 2 then 1 (community fix for washed-out CYD panels)
  Standard,  // write the standard positive / negative gamma tables
};
constexpr PanelGamma kPanelGamma = PanelGamma::Standard;
constexpr uint32_t kGammaFixSettleMs = 120;  // Reselect: once, during setup()
// ILI9341 frame rate (FRMCTR1 "RTNA" clocks per line): 0x1A ~73 Hz (the
// ILI9341_2_DRIVER init), 0x13 ~100 Hz, 0x10 ~119 Hz. A higher refresh rate
// hides the slow "rolling" shimmer of these TN panels. 0 keeps the init value.
constexpr uint8_t kPanelFrameRateRtna = 0x13;
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// ---------------------------------------------------------------------------
// Touch controller XPT2046 (bit-banged SPI, so the SD card can use VSPI)
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
// SD card (VSPI) and the photo viewer
// ---------------------------------------------------------------------------
constexpr uint8_t kSdSck = 18;
constexpr uint8_t kSdMiso = 19;
constexpr uint8_t kSdMosi = 23;
constexpr uint8_t kSdCs = 5;
constexpr uint32_t kSdFrequency = 20000000;
constexpr char kPhotoDir[] = "/photos";  // JPEG files, ideally 320 x 240 baseline
constexpr uint8_t kMaxPhotos = 64;
constexpr uint8_t kMaxPhotoPathLen = 63;
constexpr uint32_t kPhotoSlideMs = 15000;  // slideshow: next photo after this
constexpr uint32_t kPhotoHintMs = 2000;    // tap-zone hint shown on opening

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
// A status of "auth failed" / "no AP" right after WiFi.begin() may be stale
// from the previous attempt; it only counts after this grace period.
constexpr uint32_t kWifiFailGraceMs = 1000;
constexpr uint32_t kWifiScanTimeoutMs = 10000;  // async scan normally takes 2-4 s
constexpr uint8_t kMaxScanResults = 12;         // strongest networks kept from a scan
constexpr uint8_t kMaxSavedNetworks = 5;        // in NVS, most recently used first
constexpr uint8_t kMaxSsidLen = 32;             // 802.11 limit
constexpr uint8_t kMaxPasswordLen = 63;         // WPA2 passphrase limit
constexpr uint32_t kWifiDoneMessageMs = 1500;   // "Connected" shown before the main screen

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
// Cities (entered on the device, stored in NVS) and their rotation
// ---------------------------------------------------------------------------
constexpr uint8_t kMaxCities = 8;
constexpr uint8_t kMaxCityNameLen = 47;    // UTF-8 bytes
constexpr uint8_t kMaxCityRegionLen = 47;  // "admin1, country"
// Used until the user has saved any city. UTF-8; glyphs must exist in gen_fonts.py.
// Names per UI language (Vietnamese, English); id = GeoNames id of the place.
constexpr char kDefaultCityNameVi[] = "TP. Hồ Chí Minh";
constexpr char kDefaultCityNameEn[] = "Ho Chi Minh City";
constexpr char kDefaultCityRegionVi[] = "Việt Nam";
constexpr char kDefaultCityRegionEn[] = "Vietnam";
constexpr uint32_t kDefaultCityGeoId = 1566083;
constexpr float kDefaultCityLat = 10.82f;
constexpr float kDefaultCityLon = 106.63f;
// Seconds each city stays on the main screen; Settings cycles through these.
constexpr uint8_t kRotateChoicesSec[] = {10, 20, 30, 60};
constexpr uint8_t kDefaultRotateSec = 20;
constexpr uint32_t kRotatePauseMs = 60000;  // after a tap, rotation waits this long

// ---------------------------------------------------------------------------
// Open-Meteo (free, no API key; non-commercial limit 10 000 calls per day)
// ---------------------------------------------------------------------------
constexpr char kForecastUrl[] = "https://api.open-meteo.com/v1/forecast";
constexpr char kAirQualityUrl[] = "https://air-quality-api.open-meteo.com/v1/air-quality";
constexpr char kGeocodingUrl[] = "https://geocoding-api.open-meteo.com/v1/search";
// One place by GeoNames id, used to fetch a saved city's name in the other language.
constexpr char kGeocodingGetUrl[] = "https://geocoding-api.open-meteo.com/v1/get";
// Saved cities missing a name in one language are looked up one request at a
// time, kCityNameGapMs apart (each blocks the loop briefly); after a failed
// request the lookup waits kCityNameRetryMs.
constexpr uint32_t kCityNameGapMs = 3000;
constexpr uint32_t kCityNameRetryMs = 60000;
// Results searched to match an old (pre-id) saved city by its coordinates.
constexpr uint8_t kGeoMatchResults = 10;
constexpr uint8_t kForecastDays = 7;
constexpr uint8_t kForecastHours = 24;
constexpr uint8_t kHourlyStep = 3;  // the 24 h graph shows every 3rd hour
constexpr uint8_t kGeoMaxResults = 5;
constexpr uint8_t kMaxSearchLen = 40;
constexpr uint32_t kWeatherRefreshMs = 10UL * 60UL * 1000UL;  // all cities in one request
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;           // after an error
constexpr uint16_t kHttpTimeoutMs = 8000;

}  // namespace config
