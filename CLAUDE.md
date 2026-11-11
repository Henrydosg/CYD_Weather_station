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

## Boards

Two boards, one PlatformIO env each (exactly one `BOARD_*` macro, `include/board.h` picks
`include/boards/<name>.h`, `namespace board`; `config.h` reads pins / size / inversion /
touch mode from it):

| env | board | panel | touch | backlight |
|---|---|---|---|---|
| `cyd` (default) | ESP32-2432S028R 2.8" | ILI9341 320x240 | XPT2046 bit-banged on its own pins | 21 |
| `cyd35` | ESP32-3248S035R 3.5" | ST7796 480x320 | XPT2046 on the TFT bus (CS 33, IRQ 36) via `TFT_eSPI::getSPIinstance()` in a transaction | 27 |

ILI9341-only setup (gamma tables, frame rate) is skipped on other panels. The 3.5" values
(pins from rzeldent/platformio-espressif32-sunton) are not yet checked on a real board.
Multi-resolution plan: step 1 board profiles, step 2 layout scaling, step 3 fonts / icons
per size class, step 4 per-board releases (all done, 2026-11-11; the 3.5" is not yet
checked on a real board and nothing has been released for it).

Releases (step 4): `python tools/release.py [--board cyd28|cyd35] [--no-build] [--out DIR]`
builds `cyd-release` and `cyd35-release` and writes per board `firmware/<prefix>-<ver>.bin`,
`ota/<manifest>` and a web-installer manifest: 2.8" `cyd-weather-*`, `ota/latest.json`,
`manifest.json` (unchanged names: devices in use read them); 3.5" `cyd35-weather-*`,
`ota/cyd35.json`, `manifest-cyd35.json`. Bootloader / partitions / boot_app0 are shared
(release.py checks they are identical). The firmware's `kOtaManifestUrl` is
`CYD_PAGES_URL "/ota/" BOARD_OTA_MANIFEST` (board header); release.py checks it matches
its table. `docs/index.html` has a board picker (radio buttons switch the install
button's manifest). `--out` writes elsewhere for a dry run.

Size classes (step 3): `tools/gen_fonts.py`, `gen_tf_fonts.py` and `gen_icons.py` write one
source per class with the same symbol names, each wrapped in `#if` on `BOARD_CYD35`:
`fonts.cpp` / `fonts_m.cpp`, `tf_fonts.cpp` / `tf_fonts_m.cpp`, `icons.cpp` / `icons_m.cpp`
(class M = sizes x 4/3: Medium 27, Big 53, time 91 px, icons 96 / 40 px). Flash: 2.8"
~1.56 MB, 3.5" ~1.82 MB of the 1.875 MB OTA slot. The hourglass is not re-rendered at
3 px: `tf::kViewNum / kViewDen` (3/2 on the 3.5") shows the 2 px static layer and cells
x 1.5 (nearest; every cell exactly 3 px), so its RAM stays 19.9 KB (no PSRAM, TLS needs
~40 KB in one block). `ClockFace` draws its text parts through a temporary sprite in
bands of at most 12 KB (`drawPart`).

Layout (`src/ui/layout.h`): screens are written in base 320x240 pixels and every layout
constant goes through `ui::lay::X(v)` (x, widths: x W/320), `Y(v)` (y, heights: x H/240)
or `S(v)` / `Sf(v)` (radii, strokes, square sizes: the smaller factor). On the 2.8" all
three are the identity (screens unchanged); on the 3.5" X is 1.5, Y and S 4/3.
`ForBoard(base, cyd35)` overrides a value that scales badly. Sizes tied to a font or an
icon (text sprite heights, baselines, digit slots, icon sizes) use S, like the class M
fonts. New layout constants must use X / Y / S too. The clock: `tf::kScreenW/H` and
`kColumnW` follow `BOARD_CYD35` in `timefalls_config.h` (pure constexpr for the host
test), `ClockFace` scales its positions the same way. `preview_screen.py --board cyd35`
renders the 3.5" screens with the same scaling.

### 2.8" board (cyd)

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
| TFT backlight      | 21   | active HIGH, LEDC PWM (`hw/backlight`)     |
| Touch CLK          | 25   | bit-banged SPI (`ui/touch_input`)          |
| Touch MOSI         | 32   |                                            |
| Touch MISO         | 39   | input only                                 |
| Touch CS           | 33   |                                            |
| Touch IRQ          | 36   | input only, low when touched               |
| RGB LED R / G / B  | 4 / 16 / 17 | active LOW (not used yet)           |
| LDR                | 34   | ADC1, 0 dB attenuation, auto brightness    |
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
  board.h, boards/     per-board hardware profile (cyd28.h, cyd35.h), selected by the env
  config.h             all tunable constants (pins, cities, intervals, URLs, timeouts)
  secrets.example.h    credentials template (committed)
  secrets.h            default WiFi credentials (git-ignored)
src/
  main.cpp             setup()/loop() only: creates modules and wires them together
  net_worker           network task (core 1): owns the HTTPS clients (weather, geocoder, city
                       names, OTA, message); loop() sends commands, sync() copies NetSnapshot
  net/wifi_manager     non-blocking STA: saved networks then secrets.h default, backoff 5-60 s,
                       async scan, user-chosen connect (connectTo) with result for the UI
  net/remote_message   remote note from docs/message.json (every 30 min), RemoteMessage +
                       messageDue() (dates, not yet closed)
  net/time_sync        SNTP; UTC epoch for the UI (each city adds its own UTC offset)
  net/ota_updater      firmware updates over WiFi: manifest check (first after 1 min online,
                       then daily, or on request), streamed download into the other OTA slot,
                       MD5 check, restart; OtaStatus (net/ota_status.h) is plain data for the UI
  weather/weather_data.h   provider-independent CityWeather (current, 7 days, 8 x 3 h),
                           GeoResult, FetchStatus / SearchStatus, wmoCategory()
  weather/open_meteo       all cities in one forecast request + one air-quality request
  weather/geocoding        city search (Open-Meteo Geocoding API), up to 5 results;
                           readPlace() cleans names ("Ð"->"Đ", no "(...)", "Thành phố"->"TP.")
  weather/city_names       fills each saved city's name in the other language (/v1/get by id)
  weather/https_json       HTTPS GET with CA check, filtered streaming JSON parse through a
                           1 KB chunk reader (ArduinoJson alone reads the TLS stream byte by
                           byte: seconds per forecast); logs connect+GET / body ms, bytes, heap
  weather/wmo_codes        WMO weather code -> WeatherCategory (icon)
  weather/nowcast          rainOutlook(): rain soon / raining until / continues, from the
                           15-minute precipitation (minutely_15, kNowcastSlots)
  calendar/lunar_calendar  Vietnamese lunar date (new moons / solar terms, UTC+7) and days
                           to Tet; double precision, so the dashboard caches it per date
  hw/backlight             LEDC PWM backlight: Auto (LDR, smoothed, fades) or fixed levels
  sys/boot_health          reset reason, boot / crash counters (NVS "cyd-boot"), OTA rollback
                           guard (BootReport, plain data for the UI)
  weather/open_meteo_root_ca.h  ISRG Root X1 + X2 + Gen Y Root YR + YE (Let's Encrypt) for
                                *.open-meteo.com and *.github.io; mbedtls rejects the YR chain via
                                X1, so YR / YE must be trusted directly
  settings/settings.h      Settings / Language / TouchCalibration / SavedNetworks /
                           SavedCities (plain data)
  settings/saved_networks  remember (most recent first, max 5) / forget / find a network
  settings/saved_cities    add (max 8) / move up-down / remove a city (at least one stays)
  settings/settings_store  load/save Settings in NVS (Preferences, namespace "cyd-weather")
  ui/ui_state.h        UiState (main.cpp -> UI) and UiCommand / UiAction (UI -> main.cpp)
  ui/ui_controller     owns the screens, routes taps, returns UiCommand to main.cpp
  ui/screen_dashboard  main screen: one city (rotating), details column, 7-day strip
  ui/screen_graph      24 h temperature line + chance of rain for the shown city
  ui/screen_settings   rows: cities, rotation, brightness, calibration, WiFi, firmware;
                       header: "VI | EN" language toggle, "Photos" button
  ui/screen_message    remote message full screen (envelope, wrapped text, OK)
  ui/screen_update     installed version, OTA state / rolled back version, Check / Update
                       button, progress bar, "Info" header button
  ui/screen_info       device info: version (confirmed / verifying), uptime, boots, last reset,
                       WiFi, IP, MAC, free RAM, weather data age (UiState::system)
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
                       library), debounced taps with calibrated screen coordinates; polled
                       on its own task (kTouchPollMs), taps queue up for take()
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
  message.py           writes docs/message.json (checks glyphs, UTF-8 length, dates UTC+7)
  release.py           builds env cyd-release and writes docs/ (OTA manifest + web installer)
docs/                  GitHub Pages (main branch /docs): index.html web installer (ESP Web Tools),
                       manifest.json, firmware/*.bin, ota/latest.json - written by release.py
```

Data flow per `loop()`: `wifi.update()` (a scan starts only while `net.holdIfIdle()`) →
`timeSync.update()` → `net.setInputs()` / `net.sync(netView)` → `ui.update(UiState,
touch.take())` → `UiCommand` applied by `main.cpp` (`apply()`), which also persists
settings and passes city changes to the network task (`net.setCities()`; the weather
client keeps data of cities still in the list). City names the resolver fills in come
back in `NetSnapshot::namedCities` and are merged by coordinates (`mergeCityNames()`).
Tasks: loop (core 1, prio 1), `net` (core 1, prio 1, all HTTPS requests, 12 KB stack),
`touch` (core 1, prio 2), hourglass (core 0). loop, net and hourglass are on the task
watchdog.

## Screens, touch and settings

- Main (dashboard): shows one city and moves to the next every `rotateSec` (10/20/30/60 s,
  Settings). Date line: "T7, 01/11" + lunar "ÂL 23/9". Note line under "feels like":
  rain nowcast first (`rainOutlook`), else Tet day / full moon / lunar month start / days to
  Tet; redrawn only when its text changes. Header shows the city's local time (UTC + Open-Meteo `utc_offset_seconds`) and
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
  The radio does one thing at a time: while a scan runs (`WifiManager::scanBusy()`),
  weather, name lookups, OTA checks and messages wait (an HTTPS request during a scan
  fails and the scan overruns the library's 6 s limit -> "Scan failed"). An empty scan
  result is repeated `kWifiEmptyScanRetries` times; boot clears a driver-stored network.
  A connect fails only on a disconnect event for the SSID being tried (`WiFi.status()`
  keeps the previous network's result, and the library retries the very first connect
  once by itself); `WiFi.begin()` follows `kWifiSettleMs` after the disconnect and
  allows both WPA3 SAE methods (school WiFi 6 APs). The log names the reason.
- Clock: the ":" of the dashboard time blinks once a second (redrawn alone in
  `kHighlight` / `kBackground`, no flicker) to show the firmware is running.
- Photos: Settings -> "Photos" button (header, top right) sends `UiAction::ScanPhotos`;
  `main.cpp` runs `PhotoLibrary::scan()` and the viewer waits for the new
  `PhotoList::version`. Tapping the middle third goes straight back to the main screen.
  Put baseline JPEGs (ideally 320x240) in `/photos` on a FAT32 card.
  Decoding blocks the loop ~0.3-0.5 s per photo (bounded, like an HTTP request).
  `tools/make_test_chart.py` writes colour bars + a grey ramp to check gamma / tone.
- Firmware updates: bump `config::kFirmwareVersion`, run `python tools/release.py`, commit
  and push `docs/`. Devices check `config::kOtaManifestUrl` and show "New version" in
  Settings -> Firmware; the user taps Update (no automatic install). The release env
  (`RELEASE_BUILD`) compiles without `secrets.h` (release.py verifies the SSID/password are
  not in the binary) because docs/ is public; such a device starts with no WiFi and joins
  the network picked on its WiFi screen. While downloading, weather / search / name lookups
  pause. Rollback (`sys/boot_health`, since v1.1.5): `verifyRollbackLater()` returns true,
  so a firmware installed over WiFi boots "pending verify" and must get WiFi + one weather
  fetch within `kOtaVerifyWindowMs` (10 min), else it rolls back to the previous slot; a
  reset before that (crash, watchdog, power cut) also rolls back (bootloader). The failed
  version is kept in NVS and shown on the Firmware screen.
  The OTA download uses the ISRG roots of `open_meteo_root_ca.h` (*.github.io is Let's
  Encrypt) through `weather/https_json`.
- Remote message: `python tools/message.py "text" --from X [--show-from/--show-until
  YYYY-MM-DD]`, push `docs/message.json` ({id, from, text, show_from, show_until}, id 0 =
  none). The message screen opens over the dashboard, graph or photo viewer (menus wait)
  and stays until OK; the closed id is kept in NVS ("msgseen"). The text is never logged.
- Brightness: Settings row cycles Auto / High / Medium / Low (NVS "bright"); in Auto it
  shows the live level ("Auto 55%", `UiState::backlightPct`). Auto maps the
  smoothed LDR reading between `kLdrBrightRaw` and `kLdrDarkRaw` (either order) to
  `kBacklightAutoMinPct`..100 %; logs `[light] raw=` every `kLightLogMs` for tuning.
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
  [--screen main|setup|wifi|info]` with sample data and the colours read from `theme.h`.
  Keep it in sync with the screens when their layout changes.
- Partition table is `min_spiffs.csv`: two 1.875 MB OTA app slots (firmware ~1.48 MB,
  watch the headroom when adding assets). NVS is at 0x9000 as before, so settings survived
  the switch from `huge_app.csv`.
- Flicker: the dashboard redraws region by region (no `fillScreen` on rotation or new
  data); WiFi modem sleep is off (its current pulses made the backlight shimmer); panel
  frame rate `config::kPanelFrameRateRtna` (~100 Hz); SPI 40 MHz.
- RAM: no sprites. Avoid full-screen sprites (150 KB) - there is no PSRAM and TLS needs
  ~40 KB during a fetch; the parsed forecast of 8 cities needs ~20 KB of JSON heap.

## Coding rules

- **Non-blocking:** schedule with `millis()` and the rollover-safe form
  `now - since >= interval`. No `delay()` beyond a few ms, no `while` loops waiting on
  WiFi/HTTP. Every module exposes `update(now)` and is called every loop.
- Watchdog: `loop()`, the network task and the hourglass task are on the task watchdog
  (`kLoopWatchdogSec`, 30 s, panic + restart). Anything blocking must stay well below that.
- HTTPS requests run only on the network task (`net_worker`), never in `loop()`. Each is
  bounded: TLS handshake `kTlsHandshakeSec` (WiFiClientSecure's default is 120 s, which
  restarted the device on weak WiFi), `HTTPClient::GET()` `kHttpTimeoutMs`, body
  `kHttpBodyMaxMs`; the chunk reader feeds the watchdog while it waits.
  `OpenMeteoClient` sets `FetchStatus::Fetching` one pass before the request (forecast
  and air quality run in separate passes); `GeocodingClient` searches on the pass after
  `search()`, so the UI can draw first.
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

## TIME FALLS (hourglass clock = home screen)

The firmware also contains **TIME FALLS - Sand Atelier**, a falling-sand hourglass clock
(`src/timefalls/`, constants in `include/timefalls_config.h`, guide in
`README_TIMEFALLS.md`). It is the home screen, landscape like the weather station (layout
"B", 2026-11-08): a text column of `tf::kColumnW` = 176 px on the left (weekday + date
"CHỦ NHẬT  08/11", the time in Roboto Bold 68 (tight slots) with a blinking colon, a hairline cycle
progress, lunar date + note of the day from `ui/day_note` (rain nowcast in cyan, else Tet /
full moon / days to Tet, same as the dashboard), the weather: icon, temperature,
description, city), a grey gear bottom left (`ClockFace::drawGear`, opens Settings via
`UiController::enter(now, true)`), no pause / palette buttons (the clock always runs); the hourglass (42 x 84 cells, 108 x 184 px, 700 grains) on the
right. Its region and rotation box is x 176..319, full height, so a flip never covers the
text (the glass shrinks to ~78 % while sideways) and the column is only repainted when the
clock takes the screen. Tapping the time / date shows diagnostics in place of the weather; tapping the hourglass
changes the palette (not saved). Tapping the weather (or the column below it) opens the weather station (`UiController::enter`);
tapping the time / date in the dashboard header (right part) goes back, and so does the
dashboard left alone for `config::kClockReturnMs` (`UiAction::ShowClock`). A new remote
message also opens the weather UI. Calibration on first boot runs in the weather UI before
the clock takes over. Behind the clock the forecast is refreshed every 10 min as usual, but the air quality
(not shown there) only every `config::kClockAirQualityRefreshMs` (30 min). Air motes (56) stay in the hourglass region.

- **The one exception to the single loop:** `tf::HourglassApp` runs as its own FreeRTOS
  task on core 0 (`kTaskCore`, priority 1, on the task watchdog) so the sand keeps moving
  while `loop()` (core 1) blocks in HTTPS. The display belongs to one side at a time:
  `setActive(false)` -> wait for `displayReleased()` -> weather UI draws; `ShowClock` ->
  `setActive(true)` -> the clock repaints everything. While hidden the simulation and the
  minute cycle keep running without drawing. Data crosses only as plain structs under a
  mutex: `ClockWeather` (main -> clock, `shareWeather()`), taps (main polls the touch panel
  in both modes and forwards them), the weather request (clock -> main).
- The clock sets `setSwapBytes(true)` while it owns the TFT and restores `false` (the
  photo viewer relies on it). It never calls `tft.loadFont` after boot (only sprites), so
  `ui::useFont`'s cache stays right.
- RAM: static layer 33 KB, keys 6 KB, strip 6 KB, time sprites 5.5 KB (one digit cell
  36 x 54 + the colon; only changed digits are pushed), task stack 8 KB; the other column
  sprites (date, note, weather up to 17 KB) are created per draw and freed (HTTPS needs the heap). Watch
  `[diag] ... heap (min, largest)`.
- `sand_sim`, `sand_scenes`, `hourglass_cycle`, `scene_painter`, `air_field` are portable
  C++: after changing them run `python tools/host_sim/run_host_test.py` (MSVC host test +
  previews in `previews/`) as well as the build.
- Fonts: `python tools/gen_tf_fonts.py` -> `src/timefalls/assets/` (time digits only, Roboto Bold 68 px; the
  small text uses `ui/assets/fonts`). UI text of the clock comes from `tr()` (`Str::Clock*`).
