# CYD Weather Station

Weather display for the **ESP32-2432S028R** ("Cheap Yellow Display", CYD), built with
PlatformIO + Arduino framework. Data comes from [Open-Meteo](https://open-meteo.com/)
(free, no API key): forecast, air quality and geocoding APIs. Up to 8 cities, entered on
the device, are fetched together every 10 minutes; the main screen rotates through them.

## Build

Build only, to check that it compiles:

```
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run
```

(PowerShell: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`)

**Never upload or open the serial monitor** (`pio run -t upload`, `pio device monitor`).
The user uploads and monitors from VS Code themselves.

Before the first build, `include/secrets.h` must exist. Copy it from
`include/secrets.example.h` and fill in the default WiFi SSID/password. It is
git-ignored and must never be committed. Never print WiFi passwords to Serial or logs.

## Board

- MCU: ESP32-WROOM-32 (dual core 240 MHz, 4 MB flash, no PSRAM), `board = esp32dev`
- Display: 2.8" ILI9341, 240x320, used in landscape (rotation 1, 320x240). This
  panel variant ("TPM408-2.8") needs display inversion: `config::kInvertDisplay = true`,
  otherwise every colour shows as its negative. The chip is ILI9341 (vendor spec, user
  confirmed). Inversion mirrors the gamma curve, so photos look washed out (lifted
  shadows, blue tint). Two fixed settings, compared on the board by the user (keep them):
  `config::kPanelGamma = Reselect` (`ui/panel_gamma`: Driver / Standard / Reselect) and
  `config::kPhotoTone = 1` (RGB565 tone LUT in `screen_photos`, levels 0..4).
- Touch: resistive, XPT2046 controller

### GPIO

| Function           | GPIO | Notes                                      |
|--------------------|------|--------------------------------------------|
| TFT MISO           | 12   | HSPI (`USE_HSPI_PORT`)                     |
| TFT MOSI           | 13   |                                            |
| TFT SCLK           | 14   |                                            |
| TFT CS             | 15   |                                            |
| TFT DC             | 2    |                                            |
| TFT RST            | -1   | tied to EN                                 |
| TFT backlight      | 21   | active HIGH, turned on by `tft.init()`     |
| Touch CLK          | 25   | bit-banged SPI (`ui/touch_input`)          |
| Touch MOSI         | 32   |                                            |
| Touch MISO         | 39   | input only                                 |
| Touch CS           | 33   |                                            |
| Touch IRQ          | 36   | input only, low when touched               |
| RGB LED R / G / B  | 4 / 16 / 17 | active LOW (not used yet)           |
| LDR                | 34   | ADC input (not used yet)                   |
| Speaker            | 26   | DAC (not used yet)                         |
| SD card SCK/MISO/MOSI/CS | 18 / 19 / 23 / 5 | VSPI, photo viewer   |

TFT pins are set as `build_flags` in `platformio.ini` (TFT_eSPI needs them at compile
time, `USER_SETUP_LOADED=1`). All other pins live in `include/config.h`.

## Libraries (pinned in platformio.ini)

- Platform `espressif32@6.10.0` (Arduino-ESP32 2.x)
- `bodmer/TFT_eSPI@^2.5.43`: display
- `bodmer/TJpg_Decoder@1.1.0`: JPEG photos from the SD card (built-in `SD`)
- `bblanchon/ArduinoJson@^7.4.3`: JSON, **v7 API** (`JsonDocument`, `is<T>()`,
  not `StaticJsonDocument` / `DynamicJsonDocument` / `containsKey`)
- Built in: `WiFi`, `WiFiClientSecure`, `HTTPClient`, `SPI`, `SD`

## Layout

```
README.md              user guide (Vietnamese): setup, screens, photos, troubleshooting
include/
  config.h             all tunable constants (pins, cities, intervals, URLs, timeouts)
  secrets.example.h    credentials template (committed)
  secrets.h            default WiFi credentials (git-ignored)
src/
  main.cpp             setup()/loop() only: creates modules and wires them together
  net/wifi_manager     non-blocking STA: saved networks then secrets.h default, backoff 5-60 s,
                       async scan, user-chosen connect (connectTo) with result for the UI
  net/time_sync        SNTP; UTC epoch for the UI (each city adds its own UTC offset)
  weather/weather_data.h   provider-independent CityWeather (current, 7 days, 8 x 3 h),
                           GeoResult, FetchStatus / SearchStatus, wmoCategory()
  weather/open_meteo       all cities in one forecast request + one air-quality request
  weather/geocoding        city search (Open-Meteo Geocoding API), up to 5 results;
                           readPlace() cleans names ("Ð"->"Đ", no "(...)", "Thành phố"->"TP.")
  weather/city_names       fills each saved city's name in the other language (/v1/get by id)
  weather/https_json       HTTPS GET with CA check, filtered streaming JSON parse
  weather/wmo_codes        WMO weather code -> WeatherCategory (icon)
  weather/open_meteo_root_ca.h  ISRG Root X1 + X2 (Let's Encrypt) for *.open-meteo.com
  settings/settings.h      Settings / Language / TouchCalibration / SavedNetworks /
                           SavedCities (plain data)
  settings/saved_networks  remember (most recent first, max 5) / forget / find a network
  settings/saved_cities    add (max 8) / move up-down / remove a city (at least one stays)
  settings/settings_store  load/save Settings in NVS (Preferences, namespace "cyd-weather")
  ui/ui_state.h        UiState (main.cpp -> UI) and UiCommand / UiAction (UI -> main.cpp)
  ui/ui_controller     owns the screens, routes taps, returns UiCommand to main.cpp
  ui/screen_dashboard  main screen: one city (rotating), details column, 7-day strip
  ui/screen_graph      24 h temperature line + chance of rain for the shown city
  ui/screen_settings   rows: cities, rotation interval, language, calibration, WiFi
  ui/screen_cities     saved cities, "+ Add", Up / Down / Remove panel
  ui/screen_search     geocoding results with "+" to add
  ui/screen_wifi       scanned networks, 4 per page, signal/lock/saved/connected marks
  ui/screen_keyboard   on-screen keyboard (abc / ABC / 123): WiFi password or city search
  ui/screen_connect    connecting / connected / failed (Try again, Forget)
  ui/screen_calibration  two-point touch calibration (runs automatically on first boot)
  ui/strings           all on-screen text, Vietnamese + English: tr(Str::X), weather texts
                       per WMO code, weekdays, compass, UV / AQI level names
  ui/draw_utils        shared helpers: useFont (font cache), drawCard, drawPill, header,
                       drawFitted (shrink / trim UTF-8 text), signal bars, chevron
  ui/icon, weather_icons.h  alpha-blended RGB565 icons; category -> 72 px / 30 px icon
  ui/theme             navy colour set, temperatureColor(), UV / AQI levels and colours
  ui/touch_input       XPT2046 over bit-banged SPI (same raw values as the old XPT2046_Touchscreen
                       library), debounced taps with calibrated screen coordinates
  ui/screen_photos     JPEG viewer: tap-zone hint, left = previous, middle = back, right = next,
                       tone curve kPhotoTone; slideshow every kPhotoSlideMs;
                       photos larger than 320x240 scaled 1/2..1/8
  ui/panel_gamma       ILI9341 gamma tables (Driver / Standard / Reselect), config::kPanelGamma
  photos/photo_library mounts the SD card on VSPI on every scan, lists /photos/*.jpg (sorted)
  ui/assets/           GENERATED - fonts.* and icons.* (do not edit by hand)
tools/
  gen_fonts.py         Roboto (C:/Windows/Fonts, Apache 2.0) -> VLW smooth fonts
  gen_icons.py         weather icons drawn with Pillow -> RGB565 + alpha arrays
  preview_screen.py    PC previews of all screens with sample data (Pillow)
  make_test_chart.py   320x240 JPEG colour bars + grey ramp for /photos (colour / gamma check)
```

Data flow per `loop()`: `wifi.update()` → `timeSync.update()` → `weather.update()` →
`geocoder.update()` → `ui.update(UiState, touch.poll())` → `UiCommand` applied by
`main.cpp` (`apply()`), which also persists settings and passes city changes to the
weather client (`setCities()` keeps data of cities still in the list).

## Screens, touch and settings

- Main (dashboard): shows one city and moves to the next every `rotateSec` (10/20/30/60 s,
  Settings). Header shows the city's local time (UTC + Open-Meteo `utc_offset_seconds`) and
  one dot per city. Tap the header for Settings, anywhere else for the 24 h graph; any tap
  pauses the rotation for `config::kRotatePauseMs`. Tap the graph to go back. Settings,
  lists, keyboard and the graph return to the main screen after `config::kMenuIdleMs`
  without a tap (not the photo viewer or the calibration).
- Cities: Settings -> Cities. "+ Add" opens the keyboard (type without accents), the
  geocoder returns up to 5 places (names in the UI language; "Ð/ð" is normalised to
  "Đ/đ"), "+" adds one. Tap a city for Up / Down / Remove. Stored in NVS key "cities2"
  (max 8, never empty; default `config::kDefaultCity*`). Each `City` keeps its GeoNames
  id and name / region in both languages (`cityName(city, lang)`): Vietnamese with
  accents, English international names. `CityNameResolver` fetches the missing language
  one request at a time (cities from the old "cities" key are matched by name and
  coordinates first); until then the other language's name is shown.
- Touch coordinates come from a linear raw→pixel map (`TouchCalibration`). Defaults are in
  `config.h`; on first boot (nothing in NVS) the calibration screen runs automatically.
  Touch targets should be at least ~36 px (resistive panel).
- Settings live in NVS and survive firmware uploads; erasing flash resets them (and
  re-triggers calibration).
- WiFi: Settings -> WiFi row opens the scan list. Open or saved networks connect on tap,
  others ask for the password on the keyboard. A successful connect saves the network
  (NVS key "nets", max 5, most recent first; passwords in plain text, never logged). At
  boot the saved networks are tried in order, then `secrets.h`. The UI only sends
  `UiAction::ScanWifi / ConnectWifi / ForgetWifi`; `main.cpp` drives `WifiManager` and
  saves on `ConnectAttempt::Succeeded`.
- Clock: the ":" of the dashboard time blinks once a second (redrawn alone in
  `kHighlight` / `kBackground`, no flicker) to show the firmware is running.
- Photos: Settings -> "Photos" button (header, top right) sends `UiAction::ScanPhotos`;
  `main.cpp` runs `PhotoLibrary::scan()` and the viewer waits for the new
  `PhotoList::version`. Tapping the middle third goes straight back to the main screen.
  Put baseline JPEGs (ideally 320x240) in `/photos` on a FAT32 card.
  Decoding blocks the loop ~0.3-0.5 s per photo (bounded, like an HTTP request).
  `tools/make_test_chart.py` writes colour bars + a grey ramp to check gamma / tone.
- Language: `ui/strings.cpp` has one {Vietnamese, English} pair per `Str` id, in enum
  order (checked by a static_assert). Never hard-code UI text in screens. Weather texts are
  local (per WMO code), so changing language needs no refetch.

## UI assets

- Fonts and icons are generated, then committed. After changing a script, regenerate:
  `python tools/gen_fonts.py` / `python tools/gen_icons.py --preview <png>` (needs Pillow).
- Any character shown on screen must be in the font's glyph set in `gen_fonts.py`
  (`TEXT_CHARS`: ASCII + Vietnamese + `°`; `kFontBig` contains digits only). Fonts: Tiny
  12 / TinyBold 12 / Label 13 / Small 15 / Medium 20 / Big 40 px.
  Missing glyphs are silently skipped. Only precomposed (NFC) Vietnamese is supported.
- Smooth fonts: always `ui::useFont(tft, kFontX)` (shared cache across screens), never
  `tft.loadFont` directly.
  Always pass a background colour to `setTextColor(fg, bg)` so glyph edges blend correctly.
- Theme (user's choice since 2026-10-31, keep it): black dashboard like the classic CYD
  weather screens (reference photo). Pure black background (`kBackground`, also the card
  fill), white values (`kInk`), magenta labels (`kLabel`), cyan description / lows
  (`kDescription`, `kLow`), yellow time / date / temperature / highs (`kHighlight`,
  `kTemp`, `kHigh`), orange "feels like" (`kFeels`), dark graphite outlines, blue accent
  for buttons. Colours never follow weather or time of day.
  Cards are outlined (`ui::drawCard`, no shadows); text and icons on them use
  `kBackground` as their background colour (the "today" column and keys use `kTrack`).
- Icons (`gen_icons.py`): 72 px for the current weather and 30 px "...Small" for the
  7-day strip and the graph; pastel colours that read well on black.
- UI preview on PC: `python tools/preview_screen.py --out <png> [--scale 2]
  [--screen main|setup|wifi]` with sample data and the colours read from `theme.h`.
  Keep it in sync with the screens when their layout changes.
- Partition table is `huge_app.csv` (3 MB app, no OTA) to fit the assets.
- Flicker: the dashboard redraws region by region (no `fillScreen` on rotation or new
  data); WiFi modem sleep is off (its current pulses made the backlight shimmer); panel
  frame rate `config::kPanelFrameRateRtna` (~100 Hz); SPI 40 MHz.
- RAM: no sprites. Avoid full-screen sprites (150 KB) - there is no PSRAM and TLS needs
  ~40 KB during a fetch; the parsed forecast of 8 cities needs ~20 KB of JSON heap.

## Coding rules

- **Non-blocking:** schedule with `millis()` and the rollover-safe form
  `now - since >= interval`. No `delay()` beyond a few ms, no `while` loops waiting on
  WiFi/HTTP. Every module exposes `update(now)` and is called every loop.
- The one bounded exception: `HTTPClient::GET()` is synchronous and limited by
  `config::kHttpTimeoutMs`. `OpenMeteoClient` sets `FetchStatus::Fetching` one loop before
  the request (forecast and air quality run in separate loop iterations);
  `GeocodingClient` searches on the loop after `search()`, so the UI can draw first.
- **No magic numbers:** pins, intervals, coordinates and timeouts go in `config.h`
  (`namespace config`, `constexpr`, `k` prefix). Purely visual layout and colour
  constants may stay local to the `ui` module.
- **No secrets in code:** credentials only through `secrets.h` (`namespace secrets`).
- Modules don't depend on each other except through plain data (`CityWeather`,
  `GeoResult`, `WifiState`, `SavedCities`, `UiState`). Only `main.cpp` knows about all
  modules.
- Avoid `String` concatenation in hot paths; use `snprintf` into stack buffers for UI text.
- On-screen text comes from `tr()` (Vietnamese or English, UTF-8) using the smooth fonts
  in `ui/assets`.
- Serial logs: `Serial.printf("[module] ...")`, 115200 baud.
- Open-Meteo is called over HTTPS with certificate verification (`setCACert` with
  `open_meteo_root_ca.h`). Don't switch to `setInsecure()` or plain HTTP. If Open-Meteo
  changes certificate authority, update the root CA header. One location answers with a
  JSON object, several with an array (`open_meteo.cpp` handles both).
- UI code depends only on `weather_data.h`, never on provider-specific modules, so the
  weather provider can be swapped without touching `src/ui`.
- After every change, run the build command above and fix all errors and warnings in
  `src/`/`include/`.
