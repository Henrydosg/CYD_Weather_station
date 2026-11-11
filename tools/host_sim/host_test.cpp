// Host-side test of the TIME FALLS simulation, cycle timing and painter.
// Built and run by tools/host_sim/run_host_test.py (MSVC). Uses the same
// sources as the firmware (src/timefalls/sand_sim, hourglass_cycle,
// scene_painter), only the TFT / Arduino parts are left out.
//
// Checks: geometry (symmetry, no shelves, capacity), grain conservation and
// walls every few steps, drainage before the deadline in every cycle, exact
// 60 s spacing of automatic flips (with frame jitter, stalls and a millis()
// rollover), pause / resume, manual flips, and that the last rotation frame
// equals the upright frame after the remap (no pop).
#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/timefalls/air_field.h"
#include "../../src/timefalls/hourglass_cycle.h"
#include "../../src/timefalls/sand_sim.h"
#include "../../src/timefalls/scene_painter.h"

using namespace tf;

namespace {

constexpr float kPi = 3.14159265f;

int failures = 0;
#define CHECK(cond, ...)                 \
  do {                                   \
    if (!(cond)) {                       \
      ++failures;                        \
      printf("  FAIL: " __VA_ARGS__);    \
      printf("\n");                      \
    }                                    \
  } while (0)

uint32_t lcg = 12345;
uint32_t rnd(uint32_t n) {
  lcg = lcg * 1664525u + 1013904223u;
  return (lcg >> 8) % n;
}

SandSim sim;
ScenePainter painter;
uint8_t keys[ScenePainter::kKeyBytes];
uint16_t region[kSceneW * kSceneH];
uint16_t ranks[kGrains];

void writePpm(const std::string& path, const uint16_t* px, int w, int h) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) {
    return;
  }
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; ++i) {
    const uint16_t c = px[i];
    const uint8_t rgb[3] = {static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31),
                            static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63),
                            static_cast<uint8_t>((c & 0x1F) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

AirField air;

void renderRegion(float fraction, bool restOnly) {
  painter.computeKeys(sim, keys, restOnly);
  const float angle = fraction * kPi;
  painter.paintRegion(sim, keys, angle, ScenePainter::fitScale(angle), 0, kSceneH, region);
  if (fraction == 0.0f && !restOnly) {
    for (int i = 0; i < air.count(); ++i) {
      // Same 2 x 2 dots as SceneDisplay::drawAir (head only in previews).
      const int x = static_cast<int>(air.mote(i).x) / kAirDotPx * kAirDotPx;
      const int y = static_cast<int>(air.mote(i).y) / kAirDotPx * kAirDotPx;
      bool bg = true;
      for (int o = 0; o < kAirDotPx * kAirDotPx; ++o) {
        const int px = x + o % kAirDotPx, py = y + o / kAirDotPx;
        bg = bg && px < kSceneW && py < kSceneH && painter.isBackgroundPixel(px, py);
      }
      for (int o = 0; bg && o < kAirDotPx * kAirDotPx; ++o) {
        region[(y + o / kAirDotPx) * kSceneW + x + o % kAirDotPx] = air.colour(i, false);
      }
    }
  }
}

// Same pre-run as the firmware: arrival order of cycle 1 -> colour layers.
uint32_t colourPreRun(uint32_t seed) {
  sim.reset(seed);
  for (auto& r : ranks) {
    r = 0xFFFF;
  }
  sim.trackArrivals(ranks);
  HourglassCycle c;
  c.start(0);
  sim.startFlow(c.releaseStartStep(), c.releaseEndStep());
  uint32_t steps = 0;
  while (!sim.drained() && steps < HourglassCycle::msToSteps(kCycleMs)) {
    sim.step();
    ++steps;
  }
  sim.trackArrivals(nullptr);
  sim.assignLayersFromRanks(ranks);
  sim.reset(seed);
  return steps;
}

// Colour banding of the lower pile: share of neighbouring grain pairs (right,
// down) in the same layer. 1 = perfect bands, 1 / kLayers = random confetti.
float purity() {
  int same = 0, pairs = 0;
  for (int cy = kGateRow + 1; cy < SandSim::kH; ++cy) {
    for (int cx = 0; cx < SandSim::kW; ++cx) {
      const uint16_t a = sim.at(cx, cy);
      if (a == SandSim::kEmpty || a == SandSim::kWall) continue;
      const uint16_t n[2] = {sim.at(cx + 1, cy), sim.at(cx, cy + 1)};
      for (uint16_t b : n) {
        if (b == SandSim::kEmpty || b == SandSim::kWall) continue;
        ++pairs;
        same += sim.layerOf(a) == sim.layerOf(b);
      }
    }
  }
  return pairs ? static_cast<float>(same) / pairs : 0.0f;
}

struct CycleResult {
  float purity = 0;
  uint32_t lengthMs = 0;
  uint32_t passMs = 0;   // last grain through the neck
  uint32_t drainMs = 0;  // 0 = not drained
  int pileTop = 0;
  bool verified = true;
};

struct FrameRequest {
  uint32_t cycle;
  uint32_t ms;
  const char* name;
};

// aligned: from cycle 2 on, a fake synchronised wall clock (random offset)
// makes every cycle end on :00, like the firmware once SNTP has the time.
void runSeed(uint32_t seed, int cycles, bool frames, bool aligned, const std::string& outDir) {
  const uint32_t preSteps = colourPreRun(seed);
  printf("seed %08X: pre-run drained in %u steps (%.2f s)\n", seed, preSteps,
         preSteps * 1000.0f / kSimHz / 1000.0f);

  std::vector<FrameRequest> wanted;
  if (frames) {
    wanted = {{0, 300, "c01_00s3"},  {0, 8000, "c01_08s"},   {0, 20000, "c01_20s"},
              {0, 30000, "c01_30s"}, {0, 45000, "c01_45s"},  {0, 59700, "c01_59s7"},
              {1, 150, "flip_25"},   {1, 325, "flip_50"},    {1, 500, "flip_75"},
              {1, 800, "c02_00s8"},  {1, 1300, "c02_01s3"},  {1, 3000, "c02_03s"},
              {1, 30000, "c02_30s"}, {1, 59700, "c02_59s7"}, {4, 30000, "c05_30s"},
              {4, 59700, "c05_59s7"}, {9, 30000, "c10_30s"}, {9, 59700, "c10_59s7"},
              {11, 59700, "c12_59s7"}};
  }
  size_t nextFrame = 0;

  HourglassCycle cycle;
  // Start close to the 32-bit millis() rollover to exercise it.
  uint32_t now = 0xFFFFFFFFu - 90000u;
  cycle.start(now);
  sim.startFlow(cycle.releaseStartStep(), cycle.releaseEndStep());
  std::vector<CycleResult> results(cycles + 1);
  uint32_t lastStart = cycle.cycleStartMs();
  uint32_t lastLen = cycle.cycleLengthMs();
  const uint32_t wallOffset = rnd(60000);
  std::vector<uint32_t> lengths(cycles + 1, kCycleMs);
  uint32_t totalSteps = 0;
  int maxMoving = 0;
  std::string scenes;
  scenes += sceneName(sim.scene())[0];
  air.begin(seed);
  uint32_t prevNow = now;
  float airT = 0;

  while (cycle.cycleIndex() < static_cast<uint32_t>(cycles)) {
    // Frame jitter 8..45 ms, now and then a 400 ms stall.
    now += 8 + rnd(38);
    if (rnd(700) == 0) {
      now += 400;
    }
    const CycleTick t = cycle.update(now);
    const uint32_t ci = cycle.cycleIndex();
    airT += (now - prevNow) / 1000.0f;
    air.update(airT, (now - prevNow) / 1000.0f);
    prevNow = now;
    if (t.flipFinished) {
      sim.setHour(static_cast<int>(ci % 24));
      sim.flip();
      scenes += sceneName(sim.scene())[0];
      CHECK(sim.verify(), "verify after flip (cycle %u)", ci);
      sim.startFlow(cycle.releaseStartStep(), cycle.releaseEndStep());
    }
    if (t.flipStarted) {
      const uint32_t start = cycle.cycleStartMs();
      CHECK(start - lastStart == lastLen, "flip spacing %u ms, expected %u (cycle %u)",
            start - lastStart, lastLen, ci);
      if (aligned && ci >= 3) {
        CHECK((start + wallOffset) % 60000 == 0, "cycle %u did not start on :00", ci);
      }
      if (aligned && ci >= 2) {
        const uint32_t wall = now + wallOffset;
        cycle.setCycleLength(HourglassCycle::alignedLength(now - start, 60000 - wall % 60000));
      }
      lastStart = start;
      lastLen = cycle.cycleLengthMs();
      lengths[ci] = lastLen;
      CycleResult& prev = results[ci - 1];
      prev.pileTop = sim.lowerPileTop();
      prev.purity = purity();
      prev.verified = sim.verify();
      CHECK(prev.verified, "verify at boundary (cycle %u)", ci - 1);
      const uint32_t deadline = lengths[ci - 1] - kDrainMarginMs;
      CHECK(prev.drainMs != 0 && prev.drainMs <= deadline, "cycle %u drained at %u ms (deadline %u)",
            ci - 1, prev.drainMs, deadline);
      prev.lengthMs = lengths[ci - 1];
      if (ci < static_cast<uint32_t>(cycles)) {
        // Rotation: the last frame must equal the upright frame after the remap.
        if (frames && ci == 1) {
          // Catchers are replaced at the flip (they dissolve), so leave them out.
          painter.computeKeys(sim, keys, true);
          ScenePainter::fadeCatchers(keys, -1);
          painter.paintRegion(sim, keys, kPi, 1.0f, 0, kSceneH, region);
          std::vector<uint16_t> turned(region, region + kSceneW * kSceneH);
          sim.flip();
          painter.computeKeys(sim, keys, true);
          ScenePainter::fadeCatchers(keys, -1);
          painter.paintRegion(sim, keys, 0.0f, 1.0f, 0, kSceneH, region);
          CHECK(memcmp(turned.data(), region, sizeof(region)) == 0,
                "rotation end frame differs from the remapped upright frame");
          sim.flip();  // back; the cycle flips it at flipFinished
        }
      }
    }
    for (uint32_t i = 0; i < t.steps; ++i) {
      sim.step();
      ++totalSteps;
      if (sim.movingCount() > maxMoving) {
        maxMoving = sim.movingCount();
      }
      if (totalSteps % 97 == 0) {
        CHECK(sim.verify(), "verify at step %u", totalSteps);
        CHECK(sim.counts().total == kGrains, "count %u", sim.counts().total);
      }
    }
    CycleResult& cur = results[ci];
    if (cycle.phase() == CyclePhase::Flowing && cur.passMs == 0 && sim.passed() == kGrains) {
      cur.passMs = cycle.elapsedMs(now);
    }
    if (cycle.phase() == CyclePhase::Flowing && cur.drainMs == 0 && sim.drained()) {
      cur.drainMs = cycle.elapsedMs(now);
    }
    while (nextFrame < wanted.size() && wanted[nextFrame].cycle == ci &&
           cycle.elapsedMs(now) >= wanted[nextFrame].ms) {
      renderRegion(cycle.flipFraction(now), false);
      writePpm(outDir + "/" + wanted[nextFrame].name + ".ppm", region, kSceneW, kSceneH);
      ++nextFrame;
    }
    while (nextFrame < wanted.size() && wanted[nextFrame].cycle < ci) {
      ++nextFrame;  // skipped (cycle shorter than requested)
    }
  }
  int32_t worst = -100000;  // drain time minus the cycle's deadline
  int lowestTop = SandSim::kH;
  for (int i = 0; i < cycles; ++i) {
    const int32_t margin = static_cast<int32_t>(results[i].drainMs) -
                           static_cast<int32_t>(results[i].lengthMs - kDrainMarginMs);
    if (margin > worst) {
      worst = margin;
    }
    if (results[i].pileTop < lowestTop) {
      lowestTop = results[i].pileTop;
    }
  }
  printf("  %d cycles%s: closest drain %.2f s before its deadline, pile top row %d "
         "(neck gate row %d, %d rows clear), dropped steps %u, max moving %d, splashes %u\n",
         cycles, aligned ? " (aligned to :00)" : "", -worst / 1000.0f, lowestTop, kGateRow,
         lowestTop - kGateRow - 1, cycle.droppedSteps(), maxMoving, sim.splashes());
  printf("  scenes per cycle: %s, bucket tips %u\n", scenes.c_str(), sim.bucketTips());
  printf("  length/drain s:");
  for (int i = 0; i < cycles; ++i) {
    printf(" %.1f/%.2f", results[i].lengthMs / 1000.0f, results[i].drainMs / 1000.0f);
  }
  printf("\n  banding:");
  for (int i = 0; i < cycles; ++i) {
    printf(" %.2f", results[i].purity);
  }
  printf("\n");
}

// One forced scene per row: frames of a whole cycle with the air overlay.
void sceneGallery(const std::string& outDir) {
  const uint32_t times[] = {6000, 18000, 32000, 46000, 59000};
  for (int s = 0; s < kSceneCount; ++s) {
    sim.forceScene(s);
    sim.setHour(14);
    sim.reset(kSeed + s);
    air.begin(kSeed + s);
    HourglassCycle c;
    c.start(0);
    sim.startFlow(c.releaseStartStep(), c.releaseEndStep());
    uint32_t step = 0;
    for (uint32_t ms : times) {
      while (step < HourglassCycle::msToSteps(ms)) {
        sim.step();
        if (step % 2 == 0) {
          air.update(step / static_cast<float>(kSimHz), 2.0f / kSimHz);
        }
        ++step;
      }
      renderRegion(0.0f, false);
      char name[64];
      snprintf(name, sizeof(name), "/scene_%d%s_%02us.ppm", s, sceneName(static_cast<Scene>(s)),
               ms / 1000);
      writePpm(outDir + name, region, kSceneW, kSceneH);
    }
    CHECK(sim.verify(), "gallery %s conservation", sceneName(static_cast<Scene>(s)));
  }
  sim.forceScene(-1);
  sim.setHour(-1);
}

void testPauseAndManualFlip(uint32_t seed) {
  printf("pause / manual flip (seed %08X)\n", seed);
  colourPreRun(seed);
  HourglassCycle cycle;
  uint32_t now = 1000;
  cycle.start(now);
  sim.startFlow(cycle.releaseStartStep(), cycle.releaseEndStep());
  const auto run = [&](uint32_t ms) {
    const uint32_t end = now + ms;
    while (now < end) {
      now += 16;
      const CycleTick t = cycle.update(now);
      if (t.flipFinished) {
        sim.flip();
        sim.startFlow(cycle.releaseStartStep(), cycle.releaseEndStep());
      }
      for (uint32_t i = 0; i < t.steps; ++i) {
        sim.step();
      }
    }
  };
  run(20000);
  const uint32_t before = cycle.elapsedMs(now);
  const uint32_t stepsBefore = sim.stepIndex();
  cycle.setPaused(true, now);
  run(15000);
  CHECK(cycle.elapsedMs(now) == before, "elapsed moved while paused");
  CHECK(sim.stepIndex() == stepsBefore, "physics ran while paused");
  cycle.setPaused(false, now);
  run(5000);
  CHECK(cycle.elapsedMs(now) >= before + 4900, "resume lost time");
  // Manual flip mid-cycle: new cycle, everything conserved.
  const uint32_t idx = cycle.cycleIndex();
  cycle.requestFlip(now);
  run(1000);
  CHECK(cycle.cycleIndex() == idx + 1, "manual flip did not start a cycle");
  CHECK(cycle.elapsedMs(now) < 1100, "manual flip did not restart the 60 s cycle");
  CHECK(sim.verify(), "verify after manual flip");
  // Several quick manual flips.
  for (int i = 0; i < 6; ++i) {
    cycle.requestFlip(now);
    run(700 + 300 * i);
    CHECK(sim.verify(), "verify after quick flip %d", i);
  }
  run(70000);
  CHECK(sim.verify(), "verify after the manual-flip cycle");
  printf("  ok (%u cycles)\n", cycle.cycleIndex());
}

}  // namespace

int main(int argc, char** argv) {
  int seeds = 6, cycles = 12;
  std::string outDir;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      seeds = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--cycles") && i + 1 < argc) {
      cycles = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
      outDir = argv[++i];
    }
  }
  if (!sim.begin()) {
    printf("FAIL: geometry rejected by SandSim::begin()\n");
    return 1;
  }
  int capacity = 0;
  for (int cy = 0; cy <= kGateRow; ++cy) {
    capacity += sim.rowRight(cy) - sim.rowLeft(cy) + 1;
  }
  printf("geometry: %dx%d cells, chamber capacity %d, grains %u (%.0f%%)\n", kGridW, kGridH,
         capacity, kGrains, 100.0f * kGrains / capacity);
  printf("row widths (upper):");
  for (int cy = 0; cy <= kGateRow; ++cy) {
    printf(" %d", sim.rowRight(cy) - sim.rowLeft(cy) + 1);
  }
  printf("\n");
  if (!painter.begin(sim)) {
    printf("FAIL: painter allocation\n");
    return 1;
  }
  printf("painter: %u static colours, %u bytes static layer\n", painter.staticColours(),
         static_cast<unsigned>(painter.staticBytes()));
  for (float a = 0.0f; a <= 1.0f; a += 0.125f) {
    printf("  scale at %3.0f deg: %.3f\n", a * 180.0f, ScenePainter::fitScale(a * kPi));
  }

  for (int s = 0; s < seeds; ++s) {
    const uint32_t seed = kSeed + 0x9E3779B9u * s;
    runSeed(seed, cycles, s == 0 && !outDir.empty(), (s & 1) != 0, outDir);
  }
  testPauseAndManualFlip(kSeed ^ 0xABCDu);

  if (!outDir.empty()) {
    sceneGallery(outDir);
    // Palette previews of the end of cycle 1 (seed 0).
    colourPreRun(kSeed);
    HourglassCycle c;
    c.start(0);
    sim.startFlow(c.releaseStartStep(), c.releaseEndStep());
    for (uint32_t i = 0; i < HourglassCycle::msToSteps(30000); ++i) {
      sim.step();
    }
    for (uint8_t p = 0; p < kPalettes; ++p) {
      painter.setPalette(p);
      renderRegion(0.0f, false);
      writePpm(outDir + "/palette_" + ScenePainter::paletteName(p) + ".ppm", region, kSceneW,
               kSceneH);
    }
    painter.setPalette(0);
  }
  printf(failures == 0 ? "ALL CHECKS PASSED\n" : "%d CHECK(S) FAILED\n", failures);
  return failures == 0 ? 0 : 1;
}
