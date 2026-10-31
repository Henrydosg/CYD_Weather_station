# CYD Weather Station

Weather display for the **ESP32-2432S028R** ("Cheap Yellow Display", CYD), built with
PlatformIO + Arduino framework. Data comes from [Open-Meteo](https://open-meteo.com/)
(no API key). Location: Ho Chi Minh City, refreshed every 10 minutes or on screen tap.

## Build

Build only, to check that it compiles:

```
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run
```

(PowerShell: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run`)

**Never upload or open the serial monitor** (`pio run -t upload`, `pio device monitor`).
The user uploads and monitors from VS Code themselves.

Before the first build, `include/secrets.h` must exist. Copy it from
`include/secrets.example.h`. It is git-ignored and must never be committed.

## Board

- MCU: ESP32-WROOM-32 (dual core 240 MHz, 4 MB flash, no PSRAM), `board = esp32dev`
- Display: 2.8" ILI9341, 240x320, used in landscape (rotation 1, 320x240)
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
- Built in: `WiFi`, `HTTPClient`, `SPI`

## Layout

```
include/
  config.h             all tunable constants (pins, location, intervals, timeouts)
  secrets.example.h    credentials template (committed)
  secrets.h            real credentials (git-ignored)
src/
  main.cpp             setup()/loop() only: creates modules and wires them together
  net/wifi_manager     non-blocking STA state machine, reconnect with backoff 5 s to 60 s
  weather/open_meteo   HTTP GET + filtered JSON parse -> WeatherData, refresh scheduling
  weather/wmo_codes    WMO weather_code -> category + English description
  ui/screen_main       main screen, redraws only regions whose inputs changed
  ui/touch_input       XPT2046 on VSPI, debounced tap detection
```

Data flow per `loop()`: `wifi.update()` → `touch.tapped()` → `weather.update()` →
`screen.update(UiState)`.

## Coding rules

- **Non-blocking:** schedule with `millis()` and the rollover-safe form
  `now - since >= interval`. No `delay()` beyond a few ms, no `while` loops waiting on
  WiFi/HTTP. Every module exposes `update(now)` and is called every loop.
- The one bounded exception: `HTTPClient::GET()` is synchronous and limited by
  `config::kHttpTimeoutMs`. `OpenMeteoClient` sets `FetchStatus::Fetching` one loop
  before the request so the UI can show "Updating..." first.
- **No magic numbers:** pins, intervals, coordinates and timeouts go in `config.h`
  (`namespace config`, `constexpr`, `k` prefix). Purely visual layout and colour
  constants may stay local to the `ui` module.
- **No secrets in code:** credentials only through `secrets.h` (`namespace secrets`).
- Modules don't depend on each other except through plain data (`WeatherData`,
  `WifiState`, `FetchStatus`, `UiState`). Only `main.cpp` knows about all modules.
- Avoid `String` concatenation in hot paths; use `snprintf` into stack buffers for UI text.
- Built-in TFT fonts are ASCII only. On-screen text is English, no Vietnamese diacritics.
- Serial logs: `Serial.printf("[module] ...")`, 115200 baud.
- Open-Meteo is called over plain HTTP (public, non-sensitive data; avoids TLS RAM cost).
- After every change, run the build command above and fix all errors and warnings in
  `src/`/`include/`.
