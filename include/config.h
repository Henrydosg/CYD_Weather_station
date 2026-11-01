// Project-wide configuration for the CYD (ESP32-2432S028) weather station.
// Every tunable constant lives here. TFT pins are set in platformio.ini
// build_flags (TFT_eSPI requires them at compile time).
#pragma once

#include <stdint.h>

namespace config {

// ---------------------------------------------------------------------------
// Firmware version (semantic, "major.minor.patch"). Raise it for every release:
// tools/release.py reads it and devices update when the published one is newer.
// ---------------------------------------------------------------------------
constexpr char kFirmwareVersion[] = "1.1.2";

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
constexpr uint8_t kScreenRotation = 1;  // landscape, 320 x 240
// Some CYD panel variants (e.g. "TPM408-2.8") show a colour negative unless
// display inversion is enabled. Set to false if colours look inverted.
constexpr bool kInvertDisplay = true;
// ILI9341 gamma tables written after init (ui/panel_gamma). Inversion mirrors
// the gamma curve, so photos come out washed out; Reselect looked best on the
// user's board (all three compared on 2026-10-31).
enum class PanelGamma : uint8_t {
  Driver,    // the ILI9341_2_DRIVER init tables
  Standard,  // ILI9341 datasheet tables
  Reselect,  // driver tables, then GAMMASET 2 -> 1 (community CYD fix)
};
constexpr PanelGamma kPanelGamma = PanelGamma::Reselect;
constexpr uint32_t kGammaFixSettleMs = 120;  // Reselect, once during setup()
// ILI9341 frame rate (FRMCTR1 "RTNA" clocks per line): 0x1A ~73 Hz (the
// ILI9341_2_DRIVER init), 0x13 ~100 Hz, 0x10 ~119 Hz. A higher refresh rate
// hides the slow "rolling" shimmer of these TN panels. 0 keeps the init value.
constexpr uint8_t kPanelFrameRateRtna = 0x13;
constexpr int16_t kScreenWidth = 320;
constexpr int16_t kScreenHeight = 240;

// ---------------------------------------------------------------------------
// Backlight (LEDC PWM on the TFT_BL pin) and the light sensor (LDR)
// ---------------------------------------------------------------------------
constexpr uint8_t kBacklightPin = 21;  // same as TFT_BL in platformio.ini
constexpr uint8_t kBacklightPwmChannel = 7;
constexpr uint32_t kBacklightPwmHz = 5000;
constexpr uint8_t kBacklightPwmBits = 8;
// Fixed levels of Settings -> Brightness (High, Medium, Low), percent.
constexpr uint8_t kBacklightHighPct = 100;
constexpr uint8_t kBacklightMediumPct = 55;
constexpr uint8_t kBacklightLowPct = 20;
// Auto: dark room -> kBacklightAutoMinPct, bright room -> 100 %.
constexpr uint8_t kBacklightAutoMinPct = 10;
constexpr uint8_t kLdrPin = 34;  // ADC1, input only
// Raw ADC readings (0 dB attenuation) of a bright and a dark room; linear in
// between. The "[light] raw=" log line shows the value to tune these.
constexpr uint16_t kLdrBrightRaw = 20;
constexpr uint16_t kLdrDarkRaw = 400;
constexpr uint32_t kLightSampleMs = 250;
constexpr float kLightSmoothing = 0.1f;  // EMA weight of each sample (~2.5 s)
constexpr uint8_t kBacklightFadeStepPct = 2;  // per sample, so changes fade in
constexpr uint32_t kLightLogMs = 10000;       // Serial log in Auto mode, 0 = off

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
// Photo tone correction (lifted shadows / blue tint of the inverted panel):
// level 0 = off .. kPhotoToneLevels - 1 = strongest (see screen_photos.cpp).
// Level 1 looked best on the user's board with PanelGamma::Reselect.
constexpr uint8_t kPhotoToneLevels = 5;
constexpr uint8_t kPhotoTone = 1;

// ---------------------------------------------------------------------------
// OTA firmware updates (GitHub Pages of the project, written by tools/release.py)
// ---------------------------------------------------------------------------
// {"version", "url", "size", "md5"}; *.github.io chains to ISRG Root X1, which
// is already in open_meteo_root_ca.h.
constexpr char kOtaManifestUrl[] =
    "https://henrydosg.github.io/CYD_Weather_station/ota/latest.json";
constexpr uint32_t kOtaFirstCheckMs = 60000;          // after WiFi connects
constexpr uint32_t kOtaCheckIntervalMs = 86400000UL;  // then once a day
constexpr uint32_t kOtaStallTimeoutMs = 20000;        // no data while downloading
constexpr uint32_t kOtaRebootDelayMs = 2000;          // "Restarting..." stays this long
constexpr uint16_t kOtaChunkBytes = 2048;             // written per loop iteration
constexpr uint8_t kMaxVersionLen = 15;
constexpr uint8_t kMaxUrlLen = 159;

// ---------------------------------------------------------------------------
// Remote message ("Lời nhắn"): docs/message.json, written by tools/message.py
// ---------------------------------------------------------------------------
constexpr char kMessageUrl[] =
    "https://henrydosg.github.io/CYD_Weather_station/message.json";
constexpr uint32_t kMessageFirstCheckMs = 30000;     // after WiFi connects
constexpr uint32_t kMessageCheckMs = 30UL * 60000UL;  // then every 30 minutes
constexpr uint16_t kMaxMessageLen = 240;              // UTF-8 bytes
constexpr uint8_t kMaxMessageFromLen = 31;

// ---------------------------------------------------------------------------
// Lunar calendar: the Vietnamese one is computed for UTC+7
// ---------------------------------------------------------------------------
constexpr double kLunarTimeZoneHours = 7.0;

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
constexpr uint8_t kWifiEmptyScanRetries = 2;    // a scan that finds nothing is repeated
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
// Settings, lists, keyboard and the graph go back to the main screen after
// this long without a tap (the photo viewer stays: it is a photo frame).
constexpr uint32_t kMenuIdleMs = 120000;

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
// Rain nowcast: precipitation in 15-minute steps (Open-Meteo minutely_15;
// outside Europe / North America interpolated from hourly models, so "~").
constexpr uint8_t kNowcastSlots = 12;      // 3 hours
constexpr float kRainMinMm = 0.1f;         // per 15 minutes, counts as rain
constexpr uint8_t kGeoMaxResults = 5;
constexpr uint8_t kMaxSearchLen = 40;
constexpr uint32_t kWeatherRefreshMs = 10UL * 60UL * 1000UL;  // all cities in one request
constexpr uint32_t kWeatherRetryMs = 60UL * 1000UL;           // after an error
constexpr uint16_t kHttpTimeoutMs = 8000;

}  // namespace config
