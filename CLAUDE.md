# CYD Weather Station

Weather display for the **ESP32-2432S028R** ("Cheap Yellow Display", CYD), built with
PlatformIO + Arduino framework. Data comes from the
[OpenWeatherMap](https://openweathermap.org/current) Current Weather API 2.5 (free tier,
API key in `secrets.h`). Location: Ho Chi Minh City, refreshed every 10 minutes or on
screen tap.

## Build

Build only, to check that it compiles:

```
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run
```

(PowerShell: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`)

**Never upload or open the serial monitor** (`pio run -t upload`, `pio device monitor`).
The user uploads and monitors from VS Code themselves.

Before the first build, `include/secrets.h` must exist. Copy it from
`include/secrets.example.h` and fill in WiFi SSID/password and the OpenWeatherMap API
key. It is git-ignored and must never be committed. Never print the API key or the full
request URL (it contains the key) to Serial, logs or docs.

## Board

- MCU: ESP32-WROOM-32 (dual core 240 MHz, 4 MB flash, no PSRAM), `board = esp32dev`
- Display: 2.8" ILI9341, 240x320, used in landscape (rotation 1, 320x240). This
  panel variant ("TPM408-2.8") needs display inversion: `config::kInvertDisplay = true`,
  otherwise every colour shows as its negative.
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
| Touch CLK          | 25   | separate VSPI bus                          |
| Touch MOSI         | 32   |                                            |
| Touch MISO         | 39   | input only                                 |
| Touch CS           | 33   |                                            |
| Touch IRQ          | 36   | input only, low when touched               |
| RGB LED R / G / B  | 4 / 16 / 17 | active LOW (not used yet)           |
| LDR                | 34   | ADC input (not used yet)                   |
| Speaker            | 26   | DAC (not used yet)                         |
| SD card CS         | 5    | VSPI, conflicts with touch bus (not used)  |

TFT pins are set as `build_flags` in `platformio.ini` (TFT_eSPI needs them at compile
time, `USER_SETUP_LOADED=1`). All other pins live in `include/config.h`.

## Libraries (pinned in platformio.ini)

- Platform `espressif32@6.10.0` (Arduino-ESP32 2.x)
- `bodmer/TFT_eSPI@^2.5.43`: display
- `PaulStoffregen/XPT2046_Touchscreen` (git, pinned commit): touch
- `bblanchon/ArduinoJson@^7.4.3`: JSON, **v7 API** (`JsonDocument`, `is<T>()`,
  not `StaticJsonDocument` / `DynamicJsonDocument` / `containsKey`)
- Built in: `WiFi`, `WiFiClientSecure`, `HTTPClient`, `SPI`

## Layout

```
include/
  config.h             all tunable constants (pins, location, intervals, timeouts)
  secrets.example.h    credentials template (committed)
  secrets.h            WiFi credentials + OpenWeatherMap API key (git-ignored)
src/
  main.cpp             setup()/loop() only: creates modules and wires them together
  net/wifi_manager     non-blocking STA state machine, reconnect with backoff 5 s to 60 s
  net/time_sync        SNTP (UTC+7) started on each WiFi connect, local time for the clock
  weather/weather_data.h   provider-independent WeatherData / WeatherCategory / FetchStatus
  weather/openweathermap   HTTPS GET + filtered JSON parse -> WeatherData, refresh scheduling
  weather/owm_codes        OpenWeatherMap condition id -> WeatherCategory
  weather/owm_root_ca.h    USERTrust RSA root CA (valid to 2038-01-18) for TLS verification
  settings/settings.h      Settings / Language / TouchCalibration (plain data)
  settings/settings_store  load/save Settings in NVS (Preferences, namespace "cyd-weather")
  ui/ui_controller     owns the screens, routes taps, returns UiCommand to main.cpp
  ui/screen_main       main screen: weather card, detail list, footer, date; VI/EN pill + gear
  ui/screen_settings   language selector, touch calibration entry, WiFi status
  ui/screen_calibration  two-point touch calibration (runs automatically on first boot)
  ui/strings           all on-screen text, Vietnamese + English, via tr(Str::X)
  ui/draw_utils        shared helpers: useFont (font cache), drawCard, chevron
  ui/analog_clock      analog clock + digital time, rendered in sprites once per second
  ui/icon              alpha-blended RGB565 icon drawing
  ui/theme             fixed light colour set (white + ocean blue), temperatureColor()
  ui/touch_input       XPT2046 on VSPI, debounced taps with calibrated screen coordinates
  ui/assets/           GENERATED - fonts.* and icons.* (do not edit by hand)
tools/
  gen_fonts.py         Roboto (C:/Windows/Fonts, Apache 2.0) -> VLW smooth fonts
  gen_icons.py         weather icons drawn with Pillow -> RGB565 + alpha arrays
  preview_screen.py    PC preview of the main screen for several weather states (Pillow)
```

Data flow per `loop()`: `wifi.update()` → `timeSync.update()` → `weather.update()` →
`ui.update(UiState, touch.poll())` → `UiCommand` (refresh / language changed / calibration
saved) applied by `main.cpp`, which also persists settings.

## Screens, touch and settings

- Main: tap the VI/EN pill to toggle language, the gear (or signal bars) for Settings,
  anywhere else to refresh the weather.
- Touch coordinates come from a linear raw→pixel map (`TouchCalibration`). Defaults are in
  `config.h`; on first boot (nothing in NVS) the calibration screen runs automatically.
  Touch targets should be at least ~36 px (resistive panel).
- Settings live in NVS and survive firmware uploads; erasing flash resets them (and
  re-triggers calibration).
- Language: `ui/strings.cpp` has one Vietnamese and one English entry per `Str` id, in
  enum order. Never hard-code UI text in screens. Changing language re-requests the
  weather with `lang=vi|en`.

## UI assets

- Fonts and icons are generated, then committed. After changing a script, regenerate:
  `python tools/gen_fonts.py` / `python tools/gen_icons.py --preview <png>` (needs Pillow).
- Any character shown on screen must be in the font's glyph set in `gen_fonts.py`
  (`TEXT_CHARS`: ASCII + Vietnamese + `°`; `kFontTemp`/`kFontClock` contain digits only).
  Missing glyphs are silently skipped. Only precomposed (NFC) Vietnamese is supported.
- Smooth fonts: always `ui::useFont(tft, kFontX)` (shared cache across screens), never
  `tft.loadFont` directly.
  Always pass a background colour to `setTextColor(fg, bg)` so glyph edges blend correctly.
- Theme (user's choice, keep it): always light. Pure white background (`kBackground`,
  also the card fill), ocean-blue text (`kInk`), one accent blue (`kAccent`) for the
  description, date, pill and clock ring, orange `kHighlight` only for the second hand.
  Colours never follow weather or time of day. Cards are white with a thin blue outline
  and a soft navy drop shadow (`ui::drawCard` = `drawShadow` + `drawCardBody`). Shadows
  spill ~9 px below a card: when cards are close, draw all shadows first, then the bodies,
  and repaint the shadow part in a neighbouring region that gets cleared (the footer does
  this with a clipping viewport). Text and icons use `kBackground` as their background colour.
- Icons in `gen_icons.py` are coloured for the white background (pastel blue clouds,
  gold moon, navy gear); check the contrast on white when adding one.
- UI preview on PC: `python tools/preview_screen.py --out <png> [--scale 2] [--lang en]`
  renders the main screen for several weather states with the colours read from
  `theme.h`. Keep it in sync with `screen_main.cpp` when the layout changes.
- Partition table is `huge_app.csv` (3 MB app, no OTA) to fit the assets.
- RAM: the clock uses two 16-bit sprites (136x136 and 120x28, ~44 KB heap). Avoid
  full-screen sprites (150 KB) - there is no PSRAM and TLS needs ~40 KB during a fetch.

## Coding rules

- **Non-blocking:** schedule with `millis()` and the rollover-safe form
  `now - since >= interval`. No `delay()` beyond a few ms, no `while` loops waiting on
  WiFi/HTTP. Every module exposes `update(now)` and is called every loop.
- The one bounded exception: `HTTPClient::GET()` is synchronous and limited by
  `config::kHttpTimeoutMs`. `OpenWeatherMapClient` sets `FetchStatus::Fetching` one loop
  before the request so the UI can show "Updating..." first.
- **No magic numbers:** pins, intervals, coordinates and timeouts go in `config.h`
  (`namespace config`, `constexpr`, `k` prefix). Purely visual layout and colour
  constants may stay local to the `ui` module.
- **No secrets in code:** credentials only through `secrets.h` (`namespace secrets`).
- Modules don't depend on each other except through plain data (`WeatherData`,
  `WifiState`, `FetchStatus`, `UiState`). Only `main.cpp` knows about all modules.
- Avoid `String` concatenation in hot paths; use `snprintf` into stack buffers for UI text.
- On-screen text comes from `tr()` (Vietnamese or English, UTF-8) using the smooth fonts
  in `ui/assets`.
- Serial logs: `Serial.printf("[module] ...")`, 115200 baud.
- OpenWeatherMap is called over HTTPS with certificate verification (`setCACert`), since
  the API key is in the query string. Don't switch to `setInsecure()` or plain HTTP. If
  OpenWeatherMap changes its certificate chain, update `owm_root_ca.h`.
- UI code depends only on `weather_data.h`, never on provider-specific modules, so the
  weather provider can be swapped without touching `src/ui`.
- After every change, run the build command above and fix all errors and warnings in
  `src/`/`include/`.
