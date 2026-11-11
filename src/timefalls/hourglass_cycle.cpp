#include "hourglass_cycle.h"

namespace tf {

namespace {

// Cubic ease in-out on 0..1.
float easeInOutCubic(float t) {
  if (t <= 0.0f) {
    return 0.0f;
  }
  if (t >= 1.0f) {
    return 1.0f;
  }
  if (t < 0.5f) {
    return 4.0f * t * t * t;
  }
  const float u = -2.0f * t + 2.0f;
  return 1.0f - u * u * u / 2.0f;
}

}  // namespace

void HourglassCycle::start(uint32_t nowMs) {
  cycleStart_ = nowMs;
  paused_ = false;
  flipPending_ = false;
  phase_ = CyclePhase::Flowing;
  flowStartMs_ = kFirstReleaseMs;
  stepsDone_ = 0;
  cycleIndex_ = 0;
  dropped_ = 0;
  cycleMs_ = kCycleMs;
}

void HourglassCycle::setCycleLength(uint32_t ms) {
  if (ms < kMinCycleMs) ms = kMinCycleMs;
  if (ms >= kMaxCycleMs) ms = kMaxCycleMs - 1;
  cycleMs_ = ms;
}

uint32_t HourglassCycle::alignedLength(uint32_t lateMs, uint32_t msToNextMinute) {
  uint32_t len = lateMs + msToNextMinute;
  while (len < kMinCycleMs) {
    len += kCycleMs;
  }
  while (len >= kMaxCycleMs) {
    len -= kCycleMs;
  }
  return len;
}

uint32_t HourglassCycle::elapsedMs(uint32_t nowMs) const {
  return (paused_ ? pausedAt_ : nowMs) - cycleStart_;
}

float HourglassCycle::flipFraction(uint32_t nowMs) const {
  if (phase_ != CyclePhase::Rotating) {
    return 0.0f;
  }
  return easeInOutCubic(static_cast<float>(elapsedMs(nowMs)) / kFlipMs);
}

uint32_t HourglassCycle::releaseStartStep() const {
  if (firstCycle() && flowStartMs_ == kFirstReleaseMs) {
    return msToSteps(kFirstReleaseMs);
  }
  return msToSteps(kReleaseStartMs - flowStartMs_);
}

uint32_t HourglassCycle::releaseEndStep() const {
  return msToSteps(cycleMs_ - kReleaseTailMs - flowStartMs_);
}

void HourglassCycle::beginRotation(uint32_t cycleStart) {
  cycleStart_ = cycleStart;
  cycleMs_ = kCycleMs;  // until setCycleLength() says otherwise
  phase_ = CyclePhase::Rotating;
  stepsDone_ = 0;
  ++cycleIndex_;
}

void HourglassCycle::requestFlip(uint32_t nowMs) {
  if (phase_ == CyclePhase::Rotating) {
    return;
  }
  if (paused_) {
    setPaused(false, nowMs);
  }
  flipPending_ = true;
}

void HourglassCycle::setPaused(bool paused, uint32_t nowMs) {
  if (paused == paused_) {
    return;
  }
  if (paused) {
    pausedAt_ = nowMs;
  } else {
    cycleStart_ += nowMs - pausedAt_;  // resume where it stopped
  }
  paused_ = paused;
}

CycleTick HourglassCycle::update(uint32_t nowMs) {
  CycleTick t;
  if (paused_) {
    return t;
  }
  if (flipPending_) {
    flipPending_ = false;
    beginRotation(nowMs);
    t.flipStarted = true;
    return t;
  }
  uint32_t el = nowMs - cycleStart_;
  if (phase_ == CyclePhase::Rotating) {
    if (el < kFlipMs) {
      return t;
    }
    phase_ = CyclePhase::Flowing;
    flowStartMs_ = kFlipMs;
    stepsDone_ = 0;
    t.flipFinished = true;
  }
  if (el >= cycleMs_) {
    // Exactly one flip per boundary; after a stall longer than a whole cycle
    // the schedule restarts from now instead of flipping repeatedly.
    t.boundary = true;
    beginRotation(el >= 2 * cycleMs_ ? nowMs : cycleStart_ + cycleMs_);
    t.flipStarted = true;
    return t;
  }
  if (el < flowStartMs_) {
    return t;
  }
  const uint32_t due = msToSteps(el - flowStartMs_);
  if (due > stepsDone_ + kMaxStepBacklog) {
    dropped_ += due - kMaxStepBacklog - stepsDone_;
    stepsDone_ = due - kMaxStepBacklog;
  }
  uint32_t n = due - stepsDone_;
  if (n > kMaxStepsPerLoop) {
    n = kMaxStepsPerLoop;
  }
  stepsDone_ += n;
  t.steps = n;
  return t;
}

}  // namespace tf
