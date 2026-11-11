// TIME FALLS - Sand Atelier: every tunable constant of the hourglass clock.
// Pure constexpr (no Arduino headers): the simulation, timing and painter also
// compile on the PC for the host test (tools/host_sim).
// Board pins, touch calibration defaults, WiFi and SNTP settings are shared
// with the weather firmware and live in config.h.
#pragma once

#include <stdint.h>

namespace tf {

constexpr char kAppVersion[] = "1.0.0";

// ---------------------------------------------------------------------------
// Display: landscape, like the weather station (config::kScreenRotation).
// ---------------------------------------------------------------------------
// The 3.5" board (env cyd35) is 480 x 320; everything else, including the PC
// host test, uses the 2.8" 320 x 240 (must match config::kScreenWidth/Height).
#if defined(BOARD_CYD35)
constexpr int16_t kScreenW = 480;
constexpr int16_t kScreenH = 320;
// The hourglass is painted in its local pixels (2 px cells, below) and shown
// kViewNum / kViewDen larger: on the 3.5" every cell is 3 x 3 screen pixels.
// The static layer stays kAsmW x kAsmH bytes (RAM: no PSRAM, TLS needs ~40 KB).
constexpr int kViewNum = 3, kViewDen = 2;
#else
constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int kViewNum = 1, kViewDen = 1;
#endif
constexpr float kView = static_cast<float>(kViewNum) / kViewDen;

// Text column on the left (date, time, lunar note, weather, controls; ClockFace),
// the hourglass on the right. Its region (also the rotation box) is the full
// height right of the column, so a flip never draws over the text (the glass
// shrinks a little while it is sideways, ScenePainter::fitScale).
constexpr int16_t kColumnW = 176 * kScreenW / 320;  // 176 on 320 px, 264 on 480 px
constexpr int16_t kSceneX = kColumnW;
constexpr int16_t kSceneTop = 0;
constexpr int16_t kSceneW = kScreenW - kColumnW;  // 144 (2.8"), 216 (3.5")
constexpr int16_t kSceneH = kScreenH;             // 240 (2.8"), 320 (3.5")

// Hourglass assembly (glass + frame), in its own local pixel frame.
// Must be point-symmetric: the 180 degree flip reverses the grain array.
constexpr int16_t kAsmW = 108;  // glass 84 + frame
constexpr int16_t kAsmH = 184;  // glass 168 + two caps of 8
// On screen (x kView): 108 x 184 on the 2.8", 162 x 276 on the 3.5".
constexpr int16_t kAsmScreenW = kAsmW * kViewNum / kViewDen;
constexpr int16_t kAsmScreenH = kAsmH * kViewNum / kViewDen;
static_assert(kAsmW * kViewNum % kViewDen == 0 && kAsmH * kViewNum % kViewDen == 0,
              "the assembly must scale to whole pixels");
constexpr int16_t kAsmX = kSceneX + (kSceneW - kAsmScreenW) / 2;   // 194 (screen, 2.8")
constexpr int16_t kAsmY = kSceneTop + (kSceneH - kAsmScreenH) / 2;  // 28 (2.8")

// ---------------------------------------------------------------------------
// Sand grid: one cell = one grain = 2 x 2 display pixels
// ---------------------------------------------------------------------------
constexpr int kCellPx = 2;
constexpr int kGridW = 42;  // cells
constexpr int kGridH = 84;  // cells, upper chamber = rows 0..41
constexpr int kInnerLeft = (kAsmW - kGridW * kCellPx) / 2;  // 12 (local px)
constexpr int kInnerTop = (kAsmH - kGridH * kCellPx) / 2;   // 8
constexpr int kGateRow = kGridH / 2 - 1;  // last row of the upper chamber (neck)

// Glass profile, in cells, as a function of the distance d from the neck:
// throat (d <= kThroatD) of half-width kNeckHalfW, a sine flare up to
// kBulbHalfW at kBulbMaxD (slope < 1 cell per row so nothing can rest on the
// funnel walls), then a rounded shoulder down to kCapHalfW at the cap.
constexpr float kNeckHalfW = 1.0f;
constexpr float kThroatD = 2.0f;
constexpr float kBulbHalfW = 21.0f;
constexpr float kBulbMaxD = 35.0f;
constexpr float kCapHalfW = 15.0f;
constexpr int kNeckZoneRows = 4;  // rows each side of the gate counted as "neck"

// ---------------------------------------------------------------------------
// Grains
// ---------------------------------------------------------------------------
constexpr uint16_t kGrains = 700;
constexpr uint8_t kLayers = 8;  // colour bands, by order of arrival in cycle 1
constexpr uint8_t kShades = 4;  // per-grain brightness variation
constexpr uint8_t kPalettes = 4;  // SUNSET (default), DUSK, DUNE, TIDE
constexpr uint32_t kSeed = 0x5A17C0DEu;

// ---------------------------------------------------------------------------
// Physics (fixed timestep). Each grain has a velocity (vx, vy) in 1/16 cell
// per step with sub-cell remainders; it moves cell by cell through empty
// space and resolves contacts against the grid (hybrid particle / CA).
// ---------------------------------------------------------------------------
constexpr uint32_t kSimHz = 60;
constexpr int kVelShift = 4;           // 16 = one cell per step
constexpr int kGravity = 3;            // per step
constexpr int kMaxVel = 40;            // 2.5 cells / step = 300 px/s
constexpr int kTrailVel = 24;          // falling this fast leaves a dim streak
constexpr int kSparkVel = 32;          // impact speed that may light a spark
constexpr uint8_t kSparkChance = 7;    // 1 in N fast impacts
constexpr uint8_t kSparkLife = 6;      // steps (~100 ms)
constexpr uint8_t kMaxSparks = 6;
// A resting grain with an empty diagonal slides with this chance per step
// (of 256); a grain already moving always slides. Lets slopes steepen a
// little, then let go: small avalanches.
constexpr uint8_t kRestSlideChance = 72;
// Splash: an impact at least this fast (lower chamber only) bounces with
// chance kSplashChance / 256: sideways kSplashVx* % and upwards kSplashVy* %
// of the impact speed, random direction.
constexpr int kSplashVel = 24;
constexpr uint8_t kSplashChance = 110;
constexpr int kSplashVxMinPct = 30, kSplashVxMaxPct = 75;
constexpr int kSplashVyMinPct = 15, kSplashVyMaxPct = 45;
constexpr int kMaxSideVel = 28;        // |vx| cap (1.75 cells / step)
constexpr int kRollFriction = 2;       // vx lost per step when touching sand
constexpr int kAirDrag = 0;            // vx lost per step in flight
constexpr int kStreamSway = 2;         // |vx| given when a grain leaves the neck
constexpr int kNoReturnRows = 3;       // splashes stay this far below the neck
constexpr uint8_t kMaxPassPerStep = 2;  // neck throughput cap (grains / step)

// ---------------------------------------------------------------------------
// Catchers: random pins and bars in the lower chamber, a new set after every
// flip (the old set dissolves while the glass turns), faded in.
// ---------------------------------------------------------------------------
constexpr int kCatcherTopGap = 7;      // rows below the neck kept clear
constexpr int kCatcherBottomGap = 12;  // rows above the floor kept clear
constexpr int kCatcherWallGap = 3;     // cells from the glass
constexpr uint8_t kPinsMin = 2, kPinsMax = 5;
constexpr uint8_t kBarsMin = 1, kBarsMax = 2;
constexpr int kBarMinLen = 5, kBarMaxLen = 9;
constexpr uint8_t kCenterHitChance = 180;  // of 256: first catcher under the stream
constexpr int kCenterHitHalf = 3;          // ... within this many cells of the axis
constexpr uint8_t kPlaceTries = 24;
constexpr uint16_t kCatcherFadeSteps = 30;  // ~0.5 s fade-in

// Magnet scene: an attractor on a Lissajous path (cells, steps). Force on
// grains in flight = kMagnetPull * d / (d^2 + kMagnetSoft), per step, capped.
constexpr float kMagnetRowBelowGate = 19.0f;  // centre of the path
constexpr float kMagnetAxMin = 6.0f, kMagnetAxMax = 10.0f;
constexpr float kMagnetAyMin = 3.0f, kMagnetAyMax = 6.0f;
constexpr uint32_t kMagnetPeriodSteps = 1500;  // 25 s base period
constexpr float kMagnetPull = 70.0f;
constexpr float kMagnetSoft = 6.0f;
constexpr float kMagnetMaxPull = 7.0f;     // 1/16 cell per step
constexpr float kMagnetVerticalShare = 0.35f;

// Bucket scene: a cup on an axle under the stream; tips when it holds
// kBucketTipLoad grains, pours, waits, rights itself. Angles in radians.
constexpr float kBucketRowBelowGate = 10.0f;  // axle
constexpr float kBucketOuter = 5.0f, kBucketInner = 3.4f;  // half-widths (cells)
constexpr float kBucketTop = -2.6f, kBucketFloor = 1.2f, kBucketBottom = 2.8f;
constexpr int kBucketTipLoad = 12;
constexpr float kBucketTipAngle = 2.1f;      // ~120 degrees
constexpr float kBucketTipAccel = 0.006f;    // per step^2 while falling over
constexpr float kBucketReturnSpeed = 0.035f;  // per step
constexpr uint16_t kBucketHoldSteps = 45;
constexpr int kBucketKick = 10;              // vx given to the poured grains
constexpr int kDisplaceRadius = 4;           // a grain pushed by the cup moves this far at most

// Air outside the glass: motes drifting in a divergence-free (curl) flow
// field, the sum of kAirWaves travelling sine waves of a stream function,
// with a slight updraft. The flip stirs it up (gust, decays). Pixels / s.
constexpr int kAirMotes = 56;  // in the hourglass region only (144 x 240)
// A mote is a 2 x 2 dot (head) plus a 2 x 2 tail (3 x 3 on the 3.5").
constexpr int kAirDotPx = 2 * kViewNum / kViewDen;
constexpr int kAirWaves = 4;
constexpr float kAirSpeed = 16.0f;       // typical speed
constexpr float kAirRise = 3.0f;         // warm air drifting up
constexpr float kAirGust = 2.5f;         // extra speed factor right after a flip
constexpr float kAirGustSeconds = 2.0f;  // decay time
constexpr float kAirLifeMin = 6.0f, kAirLifeMax = 18.0f;  // seconds, then respawn
constexpr float kAirFadeSeconds = 1.2f;  // fade in / out over a mote's life

// Hour scene: the two digits of the hour in a 3 x 5 font, kHourScale cells
// per dot, as faint glass that sand settles on.
constexpr int kHourScale = 2;
constexpr int kHourGap = 2;                 // cells between the digits
constexpr int kHourRowBelowGate = 12;       // top of the digits
constexpr int kHourStreamOffset = 3;        // the stream lands on a digit, not the gap

// ---------------------------------------------------------------------------
// The cycle (monotonic millis, pause-aware). Nominal 60 s; once the clock is
// synchronised each cycle ends exactly on the next :00 of the real minute, so
// its length is kMinCycleMs .. kMaxCycleMs (only the first one after a sync
// differs noticeably from 60 s).
// ---------------------------------------------------------------------------
constexpr uint32_t kCycleMs = 60000;
constexpr uint32_t kMinCycleMs = 30000;
constexpr uint32_t kMaxCycleMs = 90000;    // exclusive
constexpr uint32_t kFlipMs = 650;          // eased rotation, part of the cycle
constexpr uint32_t kFirstReleaseMs = 0;    // cycle 1: flows at once
constexpr uint32_t kReleaseStartMs = 900;  // after a flip: sand settles first
constexpr uint32_t kReleaseTailMs = 2600;  // last grain passes the neck this early
constexpr uint32_t kDrainMarginMs = 500;   // all grains down and at rest by then
constexpr uint32_t kMaxStepsPerLoop = 4;
constexpr uint32_t kMaxStepBacklog = 30;   // older steps are dropped (counted)
constexpr uint32_t kPrepStepsPerLoop = 150;  // boot: colour pre-run in chunks

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
constexpr uint32_t kFrameMs = 33;  // ~30 FPS for the sand
constexpr int kBandRows = 8;       // cell rows per dirty band (16 px)
constexpr int kRegionStripRows = 20;  // rotation frame pushed in strips (holds a dirty band)
constexpr uint32_t kClockCheckMs = 50;  // colon blink follows the real second

// Touch zones of the right column are defined by its layout (ClockFace).

// ---------------------------------------------------------------------------
// The clock task inside the weather station (HourglassApp)
// ---------------------------------------------------------------------------
constexpr uint32_t kTaskStackBytes = 8192;
constexpr int kTaskCore = 0;      // the main loop (network, weather UI) runs on core 1
constexpr int kTaskPriority = 1;  // below the WiFi / TCP tasks
constexpr uint32_t kColumnRefreshMs = 1000;  // weather block / date at most this often

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
constexpr uint32_t kDiagLogMs = 5000;  // Serial, 0 = off
constexpr bool kVerifyOnFlip = true;   // full conservation check at each flip

}  // namespace tf
