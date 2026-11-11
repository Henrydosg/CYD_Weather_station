#include "sand_sim.h"

#include <math.h>
#include <string.h>

namespace tf {

namespace {

constexpr float kHalfPi = 1.57079633f;
constexpr float kEdgeEpsilon = 1e-4f;
constexpr uint32_t kShadeHash = 2654435761u;  // Knuth multiplicative hash
constexpr int kShadeShift = 30;               // top 2 bits -> 4 shades
constexpr uint32_t kByteMask = 0xFF;
constexpr uint16_t kNoRank = 0xFFFF;
constexpr int kCellUnits = 1 << kVelShift;  // velocity units per cell
constexpr int kPercent = 100;
constexpr int kRollMinVel = 8;   // |vx| needed to roll sideways on flat sand
constexpr uint8_t kFadeLevels = 4;

static_assert(kShades == 4, "shadeOf() returns 2 bits");
static_assert(kGrains < SandSim::kGhost, "grain ids must not collide with obstacles");
static_assert(kGridW % 2 == 0 && kGridH % 2 == 0, "grid must be point-symmetric");
static_assert(kMaxVel < 128 && kMaxSideVel < 128, "velocities are int8");

int sign(int v) { return (v > 0) - (v < 0); }

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Moves v towards 0 by amount (never past it).
int towardsZero(int v, int amount) {
  if (v > amount) return v - amount;
  if (v < -amount) return v + amount;
  return 0;
}

}  // namespace

float SandSim::profileHalfWidth(float d) {
  if (d <= kThroatD) {
    return kNeckHalfW;
  }
  if (d <= kBulbMaxD) {
    const float t = (d - kThroatD) / (kBulbMaxD - kThroatD);
    return kNeckHalfW + (kBulbHalfW - kNeckHalfW) * sinf(kHalfPi * t);
  }
  float t = (d - kBulbMaxD) / (kGridH / 2.0f - kBulbMaxD);
  if (t > 1.0f) {
    t = 1.0f;
  }
  return kCapHalfW + (kBulbHalfW - kCapHalfW) * sqrtf(1.0f - t * t);
}

bool SandSim::begin() {
  for (int i = 0; i < kCells; ++i) {
    grid_[i] = kWall;
  }
  uint32_t upperCapacity = 0;
  for (int cy = 0; cy < kH; ++cy) {
    // Distance of the row centre from the neck; the row must fit inside the
    // glass over its whole height, so take the narrower edge.
    const float d = fabsf(cy + 0.5f - kH / 2.0f);
    const float a = profileHalfWidth(d - 0.5f);
    const float b = profileHalfWidth(d + 0.5f);
    int hw = static_cast<int>((a < b ? a : b) + kEdgeEpsilon);
    hw = clampInt(hw, 1, kW / 2);
    left_[cy] = static_cast<uint8_t>(kW / 2 - hw);
    right_[cy] = static_cast<uint8_t>(kW / 2 + hw - 1);
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      grid_[(cy + 1) * kStride + cx + 1] = kEmpty;
    }
    if (cy <= kGateRow) {
      upperCapacity += static_cast<uint32_t>(2 * hw);
    }
  }
  // Point symmetry: the flip reverses the array.
  for (int i = 0; i < kCells / 2; ++i) {
    if ((grid_[i] == kWall) != (grid_[kCells - 1 - i] == kWall)) {
      return false;
    }
  }
  // No shelves in the upper chamber: every cell can go down or diagonally down.
  for (int cy = 0; cy < kGateRow; ++cy) {
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      const int below = (cy + 2) * kStride + cx + 1;
      if (grid_[below] == kWall && grid_[below - 1] == kWall && grid_[below + 1] == kWall) {
        return false;
      }
    }
  }
  return upperCapacity > kGrains;
}

uint32_t SandSim::rand32() {
  // xorshift32: deterministic on every platform (the boot pre-run replays it).
  uint32_t x = rng_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_ = x;
  return x;
}

int SandSim::randRange(int lo, int hi) {
  return lo + static_cast<int>(rand32() % static_cast<uint32_t>(hi - lo + 1));
}

void SandSim::reset(uint32_t seed) {
  rng_ = seed != 0 ? seed : 1;
  for (int cy = 0; cy < kH; ++cy) {
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      grid_[(cy + 1) * kStride + cx + 1] = kEmpty;
    }
  }
  memset(vx_, 0, sizeof(vx_));
  memset(vy_, 0, sizeof(vy_));
  memset(fx_, 0, sizeof(fx_));
  memset(fy_, 0, sizeof(fy_));
  memset(flags_, 0, sizeof(flags_));
  memset(stamp_, 0, sizeof(stamp_));
  tick_ = 0;
  // Stable body: rows filled from the neck upwards, the last one centred.
  uint16_t next = 0;
  for (int cy = kGateRow; cy >= 0 && next < kGrains; --cy) {
    const int n = right_[cy] - left_[cy] + 1;
    const int remaining = kGrains - next;
    const int count = remaining < n ? remaining : n;
    const int start = left_[cy] + (n - count) / 2;
    for (int i = 0; i < count; ++i) {
      grid_[(cy + 1) * kStride + start + i + 1] = static_cast<uint16_t>(++next);
    }
  }
  stepIndex_ = 0;
  passed_ = 0;
  passedAtStart_ = 0;
  moving_ = 0;
  sparks_ = 0;
  budget_ = 0;
  splashes_ = 0;
  catcherCells_ = 0;
  bucket_ = BucketState::Off;
  bucketTips_ = 0;
  placeScene();
}

void SandSim::startFlow(uint32_t releaseStartStep, uint32_t releaseEndStep) {
  stepIndex_ = 0;
  releaseStart_ = releaseStartStep;
  releaseEnd_ = releaseEndStep > releaseStartStep ? releaseEndStep : releaseStartStep + 1;
  passedAtStart_ = passed_;
}

void SandSim::assignLayersFromRanks(const uint16_t* rank) {
  for (uint16_t g = 0; g < kGrains; ++g) {
    const uint32_t r = rank == nullptr ? g : (rank[g] == kNoRank ? kGrains - 1u : rank[g]);
    layer_[g] = static_cast<uint8_t>(r * kLayers / kGrains);
  }
}

bool SandSim::interior(int cx, int cy) const {
  return cy >= 0 && cy < kH && cx >= left_[cy] && cx <= right_[cy];
}

uint16_t SandSim::at(int cx, int cy) const {
  if (cx < 0 || cx >= kW || cy < 0 || cy >= kH) {
    return kWall;
  }
  return grid_[(cy + 1) * kStride + cx + 1];
}

uint8_t SandSim::shadeOf(uint16_t cell) const {
  return static_cast<uint8_t>((static_cast<uint32_t>(cell) * kShadeHash) >> kShadeShift);
}

uint8_t SandSim::catcherFade() const {
  const uint32_t level = static_cast<uint32_t>(catcherAge_) * kFadeLevels / kCatcherFadeSteps;
  return static_cast<uint8_t>(level >= kFadeLevels ? kFadeLevels - 1 : level);
}

bool SandSim::gateAllows(int fromRow, uint16_t cell) {
  if (fromRow != kGateRow) {
    return true;
  }
  if (budget_ <= 0) {
    return false;
  }
  --budget_;
  if (rankOut_ != nullptr) {
    rankOut_[cell - 1] = passed_;
  }
  ++passed_;
  // The stream sways a little: a small random sideways speed at the neck.
  vx_[cell - 1] = static_cast<int8_t>(randRange(-kStreamSway, kStreamSway));
  return true;
}

// Down: through the gate only with budget. Up (splashes): never back into
// the neck region.
bool SandSim::verticalAllowed(int row, int dir, uint16_t cell) {
  if (dir > 0) {
    return gateAllows(row, cell);
  }
  return !(row > kGateRow && row - 1 <= kGateRow + kNoReturnRows);
}

void SandSim::moveCell(int from, int to, uint16_t cell) {
  grid_[to] = cell;
  grid_[from] = kEmpty;
}

void SandSim::step() {
  ++stepIndex_;
  ++tick_;
  if (catcherAge_ < kCatcherFadeSteps) {
    ++catcherAge_;
  }
  ++sceneStep_;
  if (scene_ == Scene::Magnet) {
    updateMagnet();
  } else if (scene_ == Scene::Bucket) {
    updateBucket();
  }
  // Release schedule: linear ramp from the grains already below the neck.
  uint32_t target = kGrains;
  if (stepIndex_ <= releaseStart_) {
    target = passedAtStart_;
  } else if (stepIndex_ < releaseEnd_) {
    target = passedAtStart_ + static_cast<uint32_t>(kGrains - passedAtStart_) *
                                  (stepIndex_ - releaseStart_) / (releaseEnd_ - releaseStart_);
  }
  budget_ = static_cast<int16_t>(clampInt(static_cast<int>(target) - passed_, 0, kMaxPassPerStep));
  moving_ = 0;

  for (int cy = kH - 1; cy >= 0; --cy) {
    const int rowBase = (cy + 1) * kStride + 1;
    const int l = left_[cy], r = right_[cy];
    const bool ltr = (rand32() & 1) != 0;
    for (int i = l; i <= r; ++i) {
      const int cx = ltr ? i : l + r - i;
      const uint16_t cell = grid_[rowBase + cx];
      if (isGrain(cell) && stamp_[cell - 1] != tick_) {
        updateGrain(rowBase + cx, cx, cy, cell);
      }
    }
  }
}

// Contact after a fall: spark and / or splash, else the grain stops falling.
void SandSim::landed(uint16_t g, int row, int impact, uint8_t& f) {
  if (impact >= kSparkVel && (f & kSparkMask) == 0 && sparks_ < kMaxSparks &&
      rand32() % kSparkChance == 0) {
    f = static_cast<uint8_t>((f & ~kSparkMask) | (kSparkLife << 4));
    ++sparks_;
  }
  if (impact >= kSplashVel && row > kGateRow + kNoReturnRows + 1 &&
      (rand32() & kByteMask) < kSplashChance) {
    const int side = (rand32() & 1) ? 1 : -1;
    const int vx = impact * randRange(kSplashVxMinPct, kSplashVxMaxPct) / kPercent;
    const int vy = impact * randRange(kSplashVyMinPct, kSplashVyMaxPct) / kPercent;
    vx_[g] = static_cast<int8_t>(clampInt(vx_[g] + side * vx, -kMaxSideVel, kMaxSideVel));
    vy_[g] = static_cast<int8_t>(-vy);
    fy_[g] = 0;
    ++splashes_;
    return;
  }
  vy_[g] = 0;
  fy_[g] = 0;
}

// Impact speed against what is below: a grain falling onto another falling
// grain only hits with the difference of their speeds.
int SandSim::relativeImpact(int vy, uint16_t blocker) const {
  if (isGrain(blocker) && vy_[blocker - 1] > 0) {
    return vy - vy_[blocker - 1];
  }
  return vy;
}

void SandSim::updateGrain(int idx, int cx, int cy, uint16_t cell) {
  const uint16_t g = cell - 1;
  stamp_[g] = tick_;
  uint8_t f = flags_[g];
  const bool wasMoving = (f & kMoved) != 0;
  f &= static_cast<uint8_t>(~kMoved);
  if (f & kSparkMask) {
    f = static_cast<uint8_t>(f - kSparkOne);
    if ((f & kSparkMask) == 0 && sparks_ > 0) {
      --sparks_;
    }
  }
  int cur = idx;
  int row = cy;
  const int below = idx + kStride;

  if (grid_[below] == kEmpty || vy_[g] < 0) {
    // --- Flight: gravity, then move cell by cell along the velocity --------
    if (scene_ == Scene::Magnet) {
      applyMagnet(g, cx, cy);
    }
    int vy = vy_[g] + kGravity;
    if (vy > kMaxVel) vy = kMaxVel;
    vx_[g] = static_cast<int8_t>(towardsZero(vx_[g], kAirDrag));
    int vx = vx_[g];
    const int ax = fx_[g] + vx, ay = fy_[g] + vy;
    int nx = ax / kCellUnits, ny = ay / kCellUnits;
    int remX = ax - nx * kCellUnits, remY = ay - ny * kCellUnits;
    if (nx == 0 && ny == 0 && vy >= 0 && grid_[below] == kEmpty) {
      ny = 1;  // a grain starting to fall always moves
      remY = 0;
    }
    const int sx = sign(nx), sy = sign(ny);
    const int mx = nx < 0 ? -nx : nx, my = ny < 0 ? -ny : ny;
    const int n = mx > my ? mx : my;
    int doneX = 0, doneY = 0;
    bool hitX = false, hitY = false;
    for (int i = 1; i <= n && !hitX && !hitY; ++i) {
      const int wantY = (my * i + n / 2) / n, wantX = (mx * i + n / 2) / n;
      if (wantY > doneY) {
        const int next = cur + sy * kStride;
        if (grid_[next] == kEmpty && verticalAllowed(row, sy, cell)) {
          cur = next;
          row += sy;
          ++doneY;
        } else {
          hitY = true;
          break;
        }
      }
      if (wantX > doneX) {
        const int next = cur + sx;
        if (grid_[next] == kEmpty) {
          cur = next;
          ++doneX;
        } else {
          hitX = true;
        }
      }
    }
    vx = vx_[g];  // the gate may have set a sway
    if (hitX) {
      vx = -vx / 2;  // bounce off glass, a catcher or sand
      remX = 0;
    }
    vx_[g] = static_cast<int8_t>(vx);
    vy_[g] = static_cast<int8_t>(vy);
    fx_[g] = static_cast<int8_t>(remX);
    fy_[g] = static_cast<int8_t>(remY);
    if (hitY) {
      if (sy < 0) {
        vy_[g] = 0;  // hit something above
        fy_[g] = 0;
      } else {
        landed(g, row, relativeImpact(vy, grid_[cur + kStride]), f);
      }
    }
    if (cur != idx) {
      moveCell(idx, cur, cell);
      f |= kMoved;
    }
  } else {
    // --- Contact: resting on something --------------------------------------
    int vx = vx_[g];
    const bool momentum = wasMoving || vx != 0 || vy_[g] > 0;
    if (vy_[g] > 0) {
      landed(g, row, relativeImpact(vy_[g], grid_[below]), f);  // came to rest
    }
    if (vy_[g] >= 0) {
      vy_[g] = 0;
      const bool canLeft = grid_[below - 1] == kEmpty;
      const bool canRight = grid_[below + 1] == kEmpty;
      int d = 0;
      if (canLeft || canRight) {
        const int pref = sign(vx);
        if (pref != 0 && (pref < 0 ? canLeft : canRight)) {
          d = pref;  // keeps rolling the way it was going
        } else if (momentum || (rand32() & kByteMask) < kRestSlideChance) {
          d = canLeft && canRight ? ((rand32() & 1) ? 1 : -1) : (canLeft ? -1 : 1);
        }
      }
      if (d != 0 && gateAllows(cy, cell)) {
        cur = below + d;
        ++row;
        moveCell(idx, cur, cell);
        f |= kMoved;
        vx = towardsZero(vx_[g], kRollFriction);
      } else if (vx != 0) {
        // Rolling on flat sand: a step sideways while fast enough.
        const int side = idx + sign(vx);
        if ((vx >= kRollMinVel || vx <= -kRollMinVel) && grid_[side] == kEmpty) {
          cur = side;
          moveCell(idx, cur, cell);
          f |= kMoved;
          vx = towardsZero(vx, 2 * kRollFriction);
        } else {
          vx = 0;
        }
      }
      vx_[g] = static_cast<int8_t>(vx);
      fx_[g] = 0;
    }
  }
  flags_[g] = f;
  if ((f & kMoved) || vx_[g] != 0 || vy_[g] != 0) {
    ++moving_;
  }
}

void SandSim::clearCatchers() {
  for (int i = 0; i < kCells; ++i) {
    if (isObstacle(grid_[i])) {
      grid_[i] = kEmpty;
    }
  }
  catcherCells_ = 0;
  bucket_ = BucketState::Off;
  bucketCount_ = 0;
}

// Places a shape if every cell and its 8 neighbours are free sand-wise, inside
// the catcher band of the lower chamber and away from the glass.
bool SandSim::placeShape(const int8_t* dx, const int8_t* dy, int n, int cx, int cy,
                         uint16_t value) {
  const int top = kGateRow + 1 + kCatcherTopGap, bottom = kH - 1 - kCatcherBottomGap;
  for (int i = 0; i < n; ++i) {
    const int x = cx + dx[i], y = cy + dy[i];
    if (y < top || y > bottom || x < left_[y] + kCatcherWallGap || x > right_[y] - kCatcherWallGap) {
      return false;
    }
    for (int oy = -1; oy <= 1; ++oy) {
      for (int ox = -1; ox <= 1; ++ox) {
        if (grid_[(y + oy + 1) * kStride + x + ox + 1] != kEmpty) {
          return false;
        }
      }
    }
  }
  for (int i = 0; i < n; ++i) {
    grid_[(cy + dy[i] + 1) * kStride + cx + dx[i] + 1] = value;
  }
  catcherCells_ = static_cast<uint16_t>(catcherCells_ + n);
  return true;
}

void SandSim::placePins() {
  int8_t dx[kMaxShapeCells], dy[kMaxShapeCells];
  const int top = kGateRow + 1 + kCatcherTopGap, bottom = kH - 1 - kCatcherBottomGap;
  const int pins = randRange(kPinsMin, kPinsMax), bars = randRange(kBarsMin, kBarsMax);
  // Most sets put one shape under the stream, to break it up.
  const int centreIndex = (rand32() & kByteMask) < kCenterHitChance
                              ? (bars > 0 && (rand32() & 1) ? pins : 0)
                              : -1;
  for (int s = 0; s < pins + bars; ++s) {
    int n = 0;
    const bool bar = s >= pins;
    if (!bar) {
      // Pin: a small diamond (radius 1 or 2); its sides are 45 degree steps.
      const int r = randRange(1, 2);
      for (int y = -r; y <= r; ++y) {
        const int w = r - (y < 0 ? -y : y);
        for (int x = -w; x <= w; ++x) {
          dx[n] = static_cast<int8_t>(x);
          dy[n] = static_cast<int8_t>(y);
          ++n;
        }
      }
    } else {
      // Bar: two cells thick (no diagonal leaks), slope 1 or 1/2, either way.
      const int len = randRange(kBarMinLen, kBarMaxLen);
      const bool steep = (rand32() & 1) != 0;
      const int dir = (rand32() & 1) ? 1 : -1;
      for (int i = 0; i < len; ++i) {
        const int y = dir * (steep ? i : i / 2);
        dx[n] = static_cast<int8_t>(i - len / 2);
        dy[n] = static_cast<int8_t>(y);
        dx[n + 1] = dx[n];
        dy[n + 1] = static_cast<int8_t>(y + 1);
        n += 2;
      }
    }
    const bool centre = s == centreIndex;
    for (int t = 0; t < kPlaceTries; ++t) {
      const int cy = randRange(top + 2, bottom - 2);
      const int cx = centre ? kW / 2 + randRange(-kCenterHitHalf, kCenterHitHalf)
                            : randRange(left_[cy], right_[cy]);
      if (placeShape(dx, dy, n, cx, centre ? randRange(top + 2, top + 12) : cy)) {
        break;
      }
    }
  }
}

void SandSim::flip() {
  clearCatchers();  // the old set dissolves while the glass turns
  // The padded grid is point-symmetric, so a 180 degree turn is a reversal.
  for (int i = 0, j = kCells - 1; i < j; ++i, --j) {
    const uint16_t t = grid_[i];
    grid_[i] = grid_[j];
    grid_[j] = t;
  }
  memset(vx_, 0, sizeof(vx_));
  memset(vy_, 0, sizeof(vy_));
  memset(fx_, 0, sizeof(fx_));
  memset(fy_, 0, sizeof(fy_));
  memset(flags_, 0, sizeof(flags_));
  sparks_ = 0;
  moving_ = 0;
  uint16_t below = 0;
  for (int cy = kGateRow + 1; cy < kH; ++cy) {
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      if (isGrain(grid_[(cy + 1) * kStride + cx + 1])) {
        ++below;
      }
    }
  }
  passed_ = below;
  passedAtStart_ = below;
  stepIndex_ = 0;
  placeScene();
}

SandCounts SandSim::counts() const {
  SandCounts c;
  for (int cy = 0; cy < kH; ++cy) {
    uint16_t n = 0;
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      if (isGrain(grid_[(cy + 1) * kStride + cx + 1])) {
        ++n;
      }
    }
    if (cy <= kGateRow - kNeckZoneRows) {
      c.upper += n;
    } else if (cy <= kGateRow + kNeckZoneRows) {
      c.neck += n;
    } else {
      c.lower += n;
    }
  }
  c.total = static_cast<uint16_t>(c.upper + c.neck + c.lower);
  return c;
}

int SandSim::lowerPileTop() const {
  for (int cy = kGateRow + 1; cy < kH; ++cy) {
    for (int cx = left_[cy]; cx <= right_[cy]; ++cx) {
      if (isGrain(grid_[(cy + 1) * kStride + cx + 1])) {
        return cy;
      }
    }
  }
  return kH;
}

bool SandSim::verify() const {
  uint8_t seen[(kGrains + 7) / 8];
  memset(seen, 0, sizeof(seen));
  uint32_t count = 0;
  for (int cy = -1; cy <= kH; ++cy) {
    for (int cx = -1; cx <= kW; ++cx) {
      const uint16_t cell = grid_[(cy + 1) * kStride + cx + 1];
      if (!interior(cx, cy)) {
        if (cell != kWall) {
          return false;  // a grain (or a hole) inside the glass wall
        }
        continue;
      }
      if (cell == kEmpty) {
        continue;
      }
      if (isObstacle(cell)) {
        if (cy <= kGateRow) {
          return false;  // catchers belong to the lower chamber only
        }
        continue;
      }
      if (cell == kWall || cell > kGrains) {
        return false;
      }
      const uint16_t g = cell - 1;
      if (seen[g >> 3] & (1 << (g & 7))) {
        return false;  // the same grain twice
      }
      seen[g >> 3] = static_cast<uint8_t>(seen[g >> 3] | (1 << (g & 7)));
      ++count;
    }
  }
  return count == kGrains;
}

}  // namespace tf
