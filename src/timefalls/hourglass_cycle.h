// The 60 s hourglass cycle: an explicit state machine on a monotonic,
// pause-aware millisecond clock. Portable (host test).
//
//   cycle 1:   Flowing from t = 0
//   cycle n>1: Rotating 0 .. kFlipMs (physics frozen), then Flowing
//   t = length:   the next cycle starts exactly one length after the previous
//                 one (cycleStart += length, no drift, at most one flip).
//
// The length is kCycleMs, or (setCycleLength) whatever makes the cycle end on
// the next :00 of the real minute once the clock is synchronised. The release
// schedule and the drain deadline follow the length.
//
// Simulation steps are due from elapsed time, not frame count; at most
// kMaxStepsPerLoop run per update, older backlog beyond kMaxStepBacklog is
// dropped (and counted) instead of an unbounded catch-up.
#pragma once

#include <stdint.h>

#include "timefalls_config.h"

namespace tf {

enum class CyclePhase : uint8_t { Flowing, Rotating };

struct CycleTick {
  uint32_t steps = 0;         // simulation steps to run now
  bool flipStarted = false;   // freeze physics, start the rotation
  bool flipFinished = false;  // remap the grains, start the flow
  bool boundary = false;      // automatic 60 s boundary reached
};

class HourglassCycle {
 public:
  void start(uint32_t nowMs);
  CycleTick update(uint32_t nowMs);

  // Manual flip: starts a rotation now and a new 60 s cycle. Ignored while
  // rotating. Also resumes a paused glass.
  void requestFlip(uint32_t nowMs);
  void setPaused(bool paused, uint32_t nowMs);
  bool paused() const { return paused_; }

  CyclePhase phase() const { return phase_; }
  uint32_t elapsedMs(uint32_t nowMs) const;  // in the current cycle, frozen when paused
  // Rotation angle as a fraction of 180 degrees (eased), 0 when flowing.
  float flipFraction(uint32_t nowMs) const;
  uint32_t cycleIndex() const { return cycleIndex_; }  // 0 = first
  bool firstCycle() const { return cycleIndex_ == 0; }
  uint32_t droppedSteps() const { return dropped_; }
  uint32_t flowStartMs() const { return flowStartMs_; }
  uint32_t cycleStartMs() const { return cycleStart_; }

  // Release schedule of the current flow phase, in steps from its start.
  uint32_t releaseStartStep() const;
  uint32_t releaseEndStep() const;

  // Length of the current cycle (call right after flipStarted / start).
  void setCycleLength(uint32_t ms);
  uint32_t cycleLengthMs() const { return cycleMs_; }
  uint32_t drainDeadlineMs() const { return cycleMs_ - kDrainMarginMs; }
  // Cycle length that ends on the next :00, given how late the cycle start
  // already is and the time left to the next real minute.
  static uint32_t alignedLength(uint32_t lateMs, uint32_t msToNextMinute);

  static uint32_t msToSteps(uint32_t ms) { return ms * kSimHz / 1000u; }

 private:
  void beginRotation(uint32_t cycleStart);

  uint32_t cycleStart_ = 0;
  uint32_t pausedAt_ = 0;
  bool paused_ = false;
  bool flipPending_ = false;
  CyclePhase phase_ = CyclePhase::Flowing;
  uint32_t flowStartMs_ = 0;
  uint32_t stepsDone_ = 0;
  uint32_t cycleIndex_ = 0;
  uint32_t cycleMs_ = kCycleMs;
  uint32_t dropped_ = 0;
};

}  // namespace tf
