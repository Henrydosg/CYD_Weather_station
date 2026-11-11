// SandSim scenes: what the lower chamber holds this minute. Chosen at random
// (never the same twice in a row) at every reset / flip, deterministic from
// the simulation's own random generator.
#include <math.h>

#include "sand_sim.h"

namespace tf {

namespace {

constexpr float kTwoPi = 6.2831853f;
constexpr int kRandScale = 1000;
constexpr int kBucketReach = 7;  // cells around the axle searched when drawing the cup
constexpr int kLoadCheckSteps = 4;
constexpr float kAngleEpsilon = 1e-3f;

// 3 x 5 dot digits, one row per 3 bits (MSB = left).
constexpr uint8_t kDigitRows[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};
constexpr int kDigitW = 3, kDigitH = 5;
constexpr int kHourMaxCells = 2 * kDigitW * kDigitH * kHourScale * kHourScale;

// Lissajous frequency pairs (x, y) for the magnet.
constexpr uint8_t kLissajous[][2] = {{1, 2}, {2, 3}, {3, 2}, {1, 3}, {3, 4}, {2, 1}};

float clampF(float v, float lim) { return v > lim ? lim : (v < -lim ? -lim : v); }

}  // namespace

const char* sceneName(Scene s) {
  switch (s) {
    case Scene::Pins:
      return "pins";
    case Scene::Magnet:
      return "magnet";
    case Scene::Bucket:
      return "bucket";
    case Scene::Hour:
      return "hour";
  }
  return "?";
}

void SandSim::placeScene() {
  catcherAge_ = 0;
  sceneStep_ = 0;
  Scene pick;
  if (forced_ >= 0) {
    pick = static_cast<Scene>(forced_ % kSceneCount);
  } else {
    Scene options[kSceneCount];
    int n = 0;
    for (uint8_t i = 0; i < kSceneCount; ++i) {
      const Scene s = static_cast<Scene>(i);
      if (s == lastScene_ || (s == Scene::Hour && hour_ < 0)) {
        continue;
      }
      options[n++] = s;
    }
    pick = options[rand32() % static_cast<uint32_t>(n)];
  }
  scene_ = pick;
  bool ok = true;
  switch (pick) {
    case Scene::Pins:
      placePins();
      break;
    case Scene::Magnet:
      startMagnet();
      break;
    case Scene::Bucket:
      ok = startBucket();
      break;
    case Scene::Hour:
      placeHour();
      ok = catcherCells_ > 0;
      break;
  }
  if (!ok) {
    clearCatchers();
    scene_ = Scene::Pins;
    placePins();
  }
  lastScene_ = scene_;
}

// --- Hour ------------------------------------------------------------------

void SandSim::placeHour() {
  if (hour_ < 0) {
    return;
  }
  int8_t dx[kHourMaxCells], dy[kHourMaxCells];
  int n = 0;
  const int digits[2] = {hour_ / 10, hour_ % 10};
  const int width = 2 * kDigitW * kHourScale + kHourGap;
  for (int k = 0; k < 2; ++k) {
    const int x0 = k * (kDigitW * kHourScale + kHourGap) - width / 2;
    for (int r = 0; r < kDigitH; ++r) {
      for (int c = 0; c < kDigitW; ++c) {
        if (!(kDigitRows[digits[k]][r] & (4 >> c))) {
          continue;
        }
        for (int sy = 0; sy < kHourScale; ++sy) {
          for (int sx = 0; sx < kHourScale; ++sx) {
            dx[n] = static_cast<int8_t>(x0 + c * kHourScale + sx);
            dy[n] = static_cast<int8_t>(r * kHourScale + sy);
            ++n;
          }
        }
      }
    }
  }
  // Off-centre, so the stream lands on a digit rather than in the gap.
  const int side = (rand32() & 1) ? 1 : -1;
  const int top = kGateRow + 1 + kHourRowBelowGate;
  const int offsets[3] = {side * kHourStreamOffset, -side * kHourStreamOffset, 0};
  for (int off : offsets) {
    if (placeShape(dx, dy, n, kW / 2 + off, top, kGhost)) {
      return;
    }
  }
}

// --- Magnet ------------------------------------------------------------------

void SandSim::startMagnet() {
  const auto frand = [this](float lo, float hi) {
    return lo + (hi - lo) * static_cast<float>(randRange(0, kRandScale)) / kRandScale;
  };
  const uint8_t* f = kLissajous[rand32() % (sizeof(kLissajous) / sizeof(kLissajous[0]))];
  magCx_ = kW / 2.0f;
  magCy_ = kGateRow + 1 + kMagnetRowBelowGate;
  magAx_ = frand(kMagnetAxMin, kMagnetAxMax);
  magAy_ = frand(kMagnetAyMin, kMagnetAyMax);
  magWx_ = kTwoPi * f[0] / kMagnetPeriodSteps;
  magWy_ = kTwoPi * f[1] / kMagnetPeriodSteps;
  magPh_ = frand(0.0f, kTwoPi);
  updateMagnet();
}

void SandSim::updateMagnet() {
  const float t = static_cast<float>(sceneStep_);
  magX_ = magCx_ + magAx_ * sinf(magWx_ * t + magPh_);
  magY_ = magCy_ + magAy_ * sinf(magWy_ * t);
}

// Pull on a grain in flight, lower chamber only; gravity always wins
// vertically. Fractions are rounded at random so weak pulls still act.
void SandSim::applyMagnet(uint16_t g, int cx, int row) {
  if (row <= kGateRow + kNoReturnRows + 1) {
    return;
  }
  const float dx = magX_ - (cx + 0.5f), dy = magY_ - (row + 0.5f);
  const float k = kMagnetPull / (dx * dx + dy * dy + kMagnetSoft);
  const float fx = clampF(k * dx, kMagnetMaxPull);
  const float fy = clampF(k * dy * kMagnetVerticalShare, kMagnetMaxPull * kMagnetVerticalShare);
  const auto stochastic = [this](float v) {
    const int whole = static_cast<int>(v);
    const float frac = fabsf(v - whole);
    const bool extra = static_cast<float>(rand32() % kRandScale) < frac * kRandScale;
    return whole + (extra ? (v < 0 ? -1 : 1) : 0);
  };
  int vx = vx_[g] + stochastic(fx);
  int vy = vy_[g] + stochastic(fy);
  vx = vx > kMaxSideVel ? kMaxSideVel : (vx < -kMaxSideVel ? -kMaxSideVel : vx);
  vy = vy > kMaxVel ? kMaxVel : (vy < -kMaxVel ? -kMaxVel : vy);
  vx_[g] = static_cast<int8_t>(vx);
  vy_[g] = static_cast<int8_t>(vy);
}

// --- Tipping bucket --------------------------------------------------------

// Cells covered by the cup at this angle (axle at bucketPx_, bucketPy_).
int SandSim::rasterBucket(float angle, uint16_t* out, int max) const {
  const float c = cosf(angle), s = sinf(angle);
  const int x0 = static_cast<int>(bucketPx_) - kBucketReach, y0 = static_cast<int>(bucketPy_) - kBucketReach;
  int n = 0;
  for (int cy = y0; cy <= y0 + 2 * kBucketReach; ++cy) {
    if (cy <= kGateRow + kNoReturnRows || cy >= kH) {
      continue;
    }
    for (int cx = x0; cx <= x0 + 2 * kBucketReach; ++cx) {
      if (!interior(cx, cy)) {
        continue;
      }
      const float dx = cx + 0.5f - bucketPx_, dy = cy + 0.5f - bucketPy_;
      const float u = c * dx + s * dy, v = -s * dx + c * dy;  // cup frame
      const float au = fabsf(u);
      const bool floor = au <= kBucketOuter && v >= kBucketFloor && v <= kBucketBottom;
      const bool wall = au >= kBucketInner && au <= kBucketOuter && v >= kBucketTop && v <= kBucketBottom;
      if ((floor || wall) && n < max) {
        out[n++] = static_cast<uint16_t>((cy + 1) * kStride + cx + 1);
      }
    }
  }
  return n;
}

// Grains in the cup (upright frame).
int SandSim::bucketLoad() const {
  int n = 0;
  for (int cy = static_cast<int>(bucketPy_ + kBucketTop); cy <= static_cast<int>(bucketPy_ + kBucketFloor); ++cy) {
    for (int cx = static_cast<int>(bucketPx_ - kBucketInner); cx <= static_cast<int>(bucketPx_ + kBucketInner); ++cx) {
      const float dx = cx + 0.5f - bucketPx_, dy = cy + 0.5f - bucketPy_;
      if (fabsf(dx) < kBucketInner && dy > kBucketTop && dy < kBucketFloor && isGrain(at(cx, cy))) {
        ++n;
      }
    }
  }
  return n;
}

// Redraws the cup at a new angle. Grains in the way are pushed to the nearest
// free cell (within kDisplaceRadius); if one has nowhere to go, nothing
// changes. Grains are never created or lost.
bool SandSim::setBucketAngle(float angle) {
  struct Change {
    uint16_t idx, old;
  };
  Change changes[3 * kBucketMaxCells];
  int nc = 0;
  uint16_t next[kBucketMaxCells];
  const int n = rasterBucket(angle, next, kBucketMaxCells);
  if (n == 0) {
    return false;
  }
  for (int i = 0; i < bucketCount_; ++i) {
    changes[nc++] = {bucketCells_[i], grid_[bucketCells_[i]]};
    grid_[bucketCells_[i]] = kEmpty;
  }
  uint16_t pending[kBucketMaxCells], pendingAt[kBucketMaxCells];
  int np = 0;
  for (int i = 0; i < n; ++i) {
    const uint16_t v = grid_[next[i]];
    if (isGrain(v)) {
      pending[np] = v;
      pendingAt[np++] = next[i];
    }
    changes[nc++] = {next[i], v};
    grid_[next[i]] = kCatcher;
  }
  bool ok = true;
  for (int p = 0; p < np && ok; ++p) {
    const int from = pendingAt[p];
    const int fy = from / kStride - 1, fx = from % kStride - 1;
    int target = -1;
    for (int r = 1; r <= kDisplaceRadius && target < 0; ++r) {
      for (int oy = -r; oy <= r && target < 0; ++oy) {
        for (int ox = -r; ox <= r; ++ox) {
          if ((ox < 0 ? -ox : ox) != r && (oy < 0 ? -oy : oy) != r) {
            continue;  // ring only
          }
          const int ty = fy + oy, tx = fx + ox;
          if (ty <= kGateRow + kNoReturnRows || !interior(tx, ty)) {
            continue;
          }
          const int t = (ty + 1) * kStride + tx + 1;
          if (grid_[t] == kEmpty) {
            target = t;
            break;
          }
        }
      }
    }
    if (target < 0) {
      ok = false;
      break;
    }
    changes[nc++] = {static_cast<uint16_t>(target), kEmpty};
    grid_[target] = pending[p];
    const uint16_t g = pending[p] - 1;
    vx_[g] = static_cast<int8_t>(bucketDir_ * kBucketKick);
    vy_[g] = 0;
  }
  if (!ok) {
    for (int i = nc - 1; i >= 0; --i) {
      grid_[changes[i].idx] = changes[i].old;
    }
    return false;
  }
  for (int i = 0; i < n; ++i) {
    bucketCells_[i] = next[i];
  }
  bucketCount_ = static_cast<uint8_t>(n);
  catcherCells_ = static_cast<uint16_t>(n);
  bucketAngle_ = angle;
  return true;
}

bool SandSim::startBucket() {
  bucketCount_ = 0;
  bucketPx_ = static_cast<float>(kW / 2 + randRange(-2, 2));
  bucketPy_ = static_cast<float>(kGateRow + 1) + kBucketRowBelowGate;
  bucketDir_ = (rand32() & 1) ? 1 : -1;
  bucketSpin_ = 0;
  bucketTimer_ = 0;
  if (!setBucketAngle(0.0f)) {
    return false;
  }
  bucket_ = BucketState::Upright;
  return true;
}

void SandSim::updateBucket() {
  switch (bucket_) {
    case BucketState::Off:
      return;
    case BucketState::Upright:
      if (sceneStep_ % kLoadCheckSteps == 0 && bucketLoad() >= kBucketTipLoad) {
        bucket_ = BucketState::Tipping;
        bucketSpin_ = 0;
        bucketDir_ = static_cast<int8_t>(-bucketDir_);  // alternate sides
      }
      return;
    case BucketState::Tipping: {
      // Falls over faster and faster, like a real cup past its balance point.
      bucketSpin_ += kBucketTipAccel;
      const float target = bucketDir_ * kBucketTipAngle;
      float next = bucketAngle_ + bucketDir_ * bucketSpin_;
      if (fabsf(next) >= kBucketTipAngle) {
        next = target;
      }
      if (!setBucketAngle(next)) {
        bucketSpin_ = 0;  // blocked by sand: try again next step
        return;
      }
      if (next == target) {
        bucket_ = BucketState::Holding;
        bucketTimer_ = kBucketHoldSteps;
        ++bucketTips_;
      }
      return;
    }
    case BucketState::Holding:
      if (--bucketTimer_ == 0) {
        bucket_ = BucketState::Returning;
      }
      return;
    case BucketState::Returning: {
      float next = bucketAngle_ - (bucketAngle_ > 0 ? kBucketReturnSpeed : -kBucketReturnSpeed);
      if (fabsf(bucketAngle_) <= kBucketReturnSpeed + kAngleEpsilon) {
        next = 0.0f;
      }
      if (setBucketAngle(next) && next == 0.0f) {
        bucket_ = BucketState::Upright;
      }
      return;
    }
  }
}

}  // namespace tf
