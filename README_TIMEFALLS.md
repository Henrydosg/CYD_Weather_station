# TIME FALLS — Sand Atelier

A falling-sand hourglass clock for the ESP32-2432S028R ("CYD"): the home screen of the
weather station firmware, landscape like it. A text column on the left carries the time
(for example `21:38`, large, the colon blinking with the seconds); the hourglass stands on
the right; the glass turns itself over every minute (exactly on :00 once the clock is set,
so every flip is a new minute). Every minute the lower chamber holds a new random scene:
brass pins and bars, an unseen magnet on a Lissajous path, a tipping bucket, or the hour
drawn in faint glass that the sand writes. Faint motes of air drift around the glass in a
swirling, divergence-free flow.

The column, top to bottom (in the weather station's language):

- The weekday and date in gold (`CHỦ NHẬT  08/11`, EN `SUNDAY  08/11`).
- The time (Roboto Bold 68, digits set tight), and under it the minute progress as a thin bronze line.
- The lunar date and the note of the day, as on the dashboard: rain soon / raining until
  (cyan) first, else Tet, full moon, the first of the lunar month, or the days to Tet.
- The weather of the saved cities in turn: small icon, temperature, description (cyan),
  city; tap it to open the weather station. Until the first forecast arrives it says
  "Đang tải thời tiết..." (nothing without WiFi). Behind the clock the forecast is refreshed
  every 10 min as in the weather station (the air quality, not shown, every 30 min), so
  opening the weather station rarely has to wait for a request.
- No pause or palette buttons: the clock always runs; tapping the hourglass changes the sand
  palette. A small grey gear in the bottom-left corner opens Settings.

## Build and upload (VS Code + PlatformIO)

One firmware, the default env `cyd` (release: `cyd-release`, built by `tools/release.py`).

```
pio run                  # build
pio run -t upload        # flash
pio device monitor       # 115200 baud, diagnostics
```

## Clock and weather station together

- The hourglass is the home screen. Tap the **weather** in the column to open the
  weather station (dashboard, graph, Settings, WiFi, firmware, photos, all as before).
  Tap the **time / date** at the right of the dashboard header to come back; left alone
  for 60 s (`config::kClockReturnMs`), the dashboard returns to the clock as well.
  A new remote message opens the weather station so it can be read.
- The clock runs as its own task on core 0, so the sand keeps falling while the weather
  station waits for an HTTPS answer on core 1. While the weather station is shown, the
  hourglass keeps running unseen: back on the clock it is at the right moment of the minute.
- On the very first boot (no touch calibration yet) the calibration runs first.

## Controls

| Where | Action |
|---|---|
| The hourglass (right half) | next palette: SUNSET (default: purple at the bottom of the pile up to gold, so splashes blend) → DUSK → DUNE → TIDE: the same grains, recoloured by layer index (not saved: SUNSET again after a restart) |
| Gear (bottom-left corner) | opens Settings directly (Back goes to the weather station; left alone it returns to the clock) |
| Weather (lunar line down to the bottom of the column, except the gear) | opens the weather station |
| Weather station: time / date (header, right) | back to the hourglass |
| Time / date | diagnostics in place of the weather on/off (fps, ms per step, heap / largest block, scene, upper/neck/lower) |

There is no flip button: the glass turns by itself every minute. Taps are debounced at
300 ms; the hourglass itself is not a control.

## The cycle: one flip per minute

- Cycle 1 starts flowing at once and lasts 60 s. At boot, cycle 1 is simulated once
  (deterministic seed, about 3 s behind the splash) and each grain's colour band (8 colours)
  becomes its order of arrival, so the first deposit forms clean layers. Colours never
  change after that; the splashes then mix them into a many-coloured sand.
- Each later cycle starts with a 180° turn in 650 ms (eased, slight scale dip so it never
  clips; physics frozen). Then the grid is reversed (exact for the point-symmetric glass),
  and the sand slumps into the funnel. Once SNTP has the time, each cycle's length is set
  when it starts so that it ends on the next :00 of the real minute. In practice that is
  60 s ± a few ms; only the first cycle after a sync is 30–90 s. Without the time, every
  cycle is 60 s. Flips follow the schedule (`cycleStart += length`: no drift, never more
  than one flip per boundary). The hourglass never waits for the network.
- The neck is metered: grains cross on a linear schedule that ends 2.6 s before the end of
  the cycle. The neck can carry about 5× that rate. Everything is down and at rest by
  0.5 s before the flip (host test: 0.73 s or more to spare, every cycle). The full lower
  chamber then shows until the flip.
- Steps are driven by elapsed time at 60 Hz. At most 4 steps run per loop; a backlog
  over 30 steps is dropped and counted, so there is no catch-up spiral.
- Serial log: `[cycle] #n starts: 60.00 s, ends on :00`, `[cycle] #n end: drained at
  57.9 s (deadline 59.5 s), pile top row 66, total conserved`, plus `[diag]` every 5 s.

## Sand physics

- Cellular automaton plus per-grain velocity (vx, vy in 1/16 cell per step, with sub-cell
  remainders). A grain in flight moves cell by cell along its velocity until something
  blocks it. On impact it may spark briefly or **splash**: bounce sideways and up, with a
  random share of the impact speed. In contact it slides diagonally (randomly, or the way
  it is rolling) or rolls a little on flat sand, else it rests. Glass and pins bounce it
  back sideways. Rows are visited bottom-up in a random direction, and a per-step stamp
  stops any grain from being updated twice in a step.
- The stream sways a little (a small random vx at the neck). Splashes never climb back
  into the neck.
## Scenes (one per minute, random, never the same twice in a row)

| Scene | What happens |
|---|---|
| **Pins** | 3–6 small brass diamonds and 1–2 bars (two cells thick, slope 1 or 1/2, either way); usually one under the stream. Splashes, split streams, little ledges of sand. |
| **Magnet** | An unseen attractor travels a Lissajous path (random frequency pair, amplitude and phase) with a faint violet glint. It pulls on grains in flight, bending the stream into curves and spreading the pile; gravity always wins vertically. |
| **Bucket** | A brass cup on an axle under the stream. When it holds 16 grains it falls over (accelerating), pours to one side, holds, then rights itself; it alternates sides. Grains in the cup's path are pushed to the nearest free cell, and if one has nowhere to go the cup waits, so no grain is ever lost. |
| **Hour** | The hour (two digits, 3×5 dots, 2 cells per dot) drawn in faint glass a little off-centre. Sand settles on the strokes and writes the hour, which the rising pile later buries. Needs the clock set (not in cycle 1). |

Scenes fade in over ~0.5 s. During the flip the old scene fades out and dissolves, and the
grains it held fall free, so the upper chamber never has obstacles. Log: `[scene] bucket`.

## Air around the glass

170 motes, each a head pixel plus a dimmer tail, in pale teal, lavender or ivory, drawn only
on the plain background (they pass behind the glass and the frame). Their velocity is the
curl of a stream function made of 4 travelling sine waves (random directions, wavelengths
of 110–260 px and slow phase drift), plus a gentle updraft. The swirls form and dissolve
and never repeat exactly. Each flip stirs the air (a gust that decays over ~2 s).
Brightness follows speed; motes fade in and out over a 6–18 s life. Only changed pixels are
written; the air is hidden during the rotation.

## Hardware used (same as the weather station in this folder)

- ESP32-WROOM-32, 4 MB flash, **no PSRAM**; `board = esp32dev`, `min_spiffs.csv`
- ILI9341 240×320 on HSPI (MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST tied to EN),
  40 MHz, `invertDisplay(true)`, gamma "Reselect", frame rate RTNA 0x13. Landscape
  (`config::kScreenRotation`, as the weather station).
- Backlight GPIO 21 (LEDC PWM, Auto via the LDR on GPIO 34)
- XPT2046 touch, bit-banged (CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36). The calibration
  saved by the weather firmware (NVS `cyd-weather`, read only) is used as is. Saved WiFi networks and the brightness mode are read from the same place.

## Rendering and memory

- The screen is never cleared after boot. Upright frames push only the changed cells (dirty
  spans per 8-row band) at ~30 fps. Rotation frames repaint the 240×242 scene in
  16-row strips.
- The static glass and frame are anti-aliased once at boot into an 8-bit indexed layer
  (33 KB). Other buffers: 6 KB of cell keys, a 7.7 KB strip, and ~34 KB of text sprites,
  all allocated once and checked. Sand grid 13 KB static.
- Grain tones (resting / moving brighter / short impact spark / dim trail) are picked at
  render time; the grain's colour layer is never changed.

## Main tuning parameters (`include/timefalls_config.h`)

`kGrains` (700, 63 % of a chamber), grid 42×84 cells, glass profile (`kNeckHalfW`,
`kBulbHalfW`, `kBulbMaxD`, `kCapHalfW`), gravity / `kMaxVel`, `kRestSlideChance`
(avalanches), splash (`kSplashVel`, `kSplashChance`, `kSplashVx*` / `kSplashVy*`,
`kRollFriction`), `kStreamSway`, pins (`kPinsMin/Max`, `kBarsMin/Max`, `kBar*Len`,
`kCenterHitChance`), magnet (`kMagnet*`), bucket (`kBucket*`, `kDisplaceRadius`), hour
(`kHour*`), air (`kAir*`), `kReleaseTailMs`, `kDrainMarginMs`, `kFlipMs`, `kFrameMs`. Colours are at the top of
`src/timefalls/scene_painter.cpp`. Fonts: `python tools/gen_tf_fonts.py`.

## Checks performed

- `pio run -e cyd` and `-e cyd-release`: build, no warnings (static RAM 30.4 %, flash 78.2 %
  of the 1.875 MB OTA slot).
- Host test (`python tools/host_sim/run_host_test.py`, MSVC, same sim / cycle / painter
  sources), run with 12 seeds × 12 cycles, frame jitter of 8–45 ms, random 400 ms
  stalls, and a `millis()` rollover. Half of the runs use a simulated synced clock with
  a random offset.
  - every grain conserved and nothing inside a wall (checked every 97 steps and at every
    flip); catchers only ever in the lower chamber;
  - flips exactly one cycle length apart; aligned runs start every cycle on :00;
  - every cycle (60 s, and the 30–90 s alignment cycles) at rest at least 0.73 s before
    its deadline, with all four scenes appearing (the bucket tipped 48–111 times per run);
  - the lower pile stays at least 7 rows clear of the neck;
  - the geometry is point-symmetric with no shelves;
  - the 180° rotation frame equals the remapped upright frame pixel for pixel, catchers
    aside, since they are replaced at the flip;
  - pause freezes time and physics; manual flips (including quick repeats) conserve every grain.
  - PC previews: `previews/timefalls_*.png`, with one row per scene in
    `previews/timefalls_scenes.png` (a mock: the text column is drawn by Pillow).

## Still to check on the device (not done yet)

1. Colour order and inversion: the 8 sand colours, the brass pins / bars / bucket, the hour
   digits (faint, but should be visible), the magnet glint, the air motes (subtle, not noisy),
   the gold frame and the dark navy background.
2. Touch: the hourglass (palette), the weather block, the time tap (watch `[tf] tap x,y`).
3. The flip: the glass turns in its region right of the text (it shrinks to ~78 % while sideways); the text never flickers. It should look smooth.
4. No flashing or tearing during sand frames and during the flip.
5. Smoothness: measured fps, render and sim times from `[diag]`. Not measured yet.
6. Every cycle drains before its deadline (`[cycle]` lines), for at least 10 consecutive
   automatic cycles; with WiFi, the flip lands as the minute digit changes, and the colon blinks.
7. Offline operation (WiFi off): the hourglass keeps running and the column shows
   `OFFLINE - TIME NOT SET`. Then the correct UTC+7 time once it syncs.
8. Free heap stable over an hour, and HTTPS fetches still succeed while the clock is shown
   (`[diag] heap ... (min ..., largest ...)`, `[weather]` lines).
9. The sand keeps moving during a weather fetch (no freeze every 10 minutes).
10. Switching: weather block -> dashboard; 60 s idle -> clock; Settings / WiFi / photos work
    as before; the photo colours are unchanged (swap-bytes handover).
