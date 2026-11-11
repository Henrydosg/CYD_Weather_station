// Falling sand inside a point-symmetric hourglass: a cellular automaton with
// per-grain velocity (hybrid particle / CA). Portable C++ (no Arduino): also
// built by the host test in tools/host_sim.
//
// Every cell holds 0 (empty), kWall (glass), kCatcher (a pin or bar in the
// lower chamber) or a grain id + 1. Grains keep their id, colour layer and
// shade for ever; velocity, sub-cell remainders and flags belong to the id.
//
// Each step, rows are visited bottom-up in a random direction. A grain in
// flight (empty below, or rising) moves cell by cell along its velocity until
// something blocks it; an impact may spark or splash (bounce sideways and up).
// A grain in contact slides diagonally (randomly, or in the direction it is
// rolling) or rolls a little on flat sand, else it rests. A per-step stamp
// makes sure no grain is updated twice in one step, whichever way it moved.
//
// Flow through the neck is metered: a grain may cross from the gate row into
// the lower chamber only while the number passed is below the release
// schedule (linear between two steps). The neck itself can carry several
// times the scheduled rate. Grains never move back up into the neck.
#pragma once

#include <stdint.h>

#include "timefalls_config.h"

namespace tf {

// What the lower chamber holds this minute (a new one after every flip).
enum class Scene : uint8_t {
  Pins,    // brass pins and bars: splashes, split streams
  Magnet,  // an unseen magnet on a Lissajous path bends the falling sand
  Bucket,  // a tipping bucket fills, tips over, pours, rights itself
  Hour,    // the hour, drawn in faint glass: sand settles on it and writes it
};
constexpr uint8_t kSceneCount = 4;
const char* sceneName(Scene s);

struct SandCounts {
  uint16_t upper = 0;
  uint16_t neck = 0;
  uint16_t lower = 0;
  uint16_t total = 0;
};

class SandSim {
 public:
  static constexpr int kW = kGridW;
  static constexpr int kH = kGridH;
  static constexpr int kStride = kW + 2;  // one wall cell of padding each side
  static constexpr int kCells = (kH + 2) * kStride;
  static constexpr uint16_t kEmpty = 0;
  static constexpr uint16_t kWall = 0xFFFF;
  static constexpr uint16_t kCatcher = 0xFFFE;  // brass pins, bars, bucket
  static constexpr uint16_t kGhost = 0xFFFD;    // faint glass (hour digits)
  static bool isGrain(uint16_t cell) { return cell != kEmpty && cell < kGhost; }
  static bool isObstacle(uint16_t cell) { return cell == kCatcher || cell == kGhost; }

  // Builds the glass mask. False if the geometry is unusable (asymmetric, a
  // shelf that could trap grains, or not enough room for kGrains).
  bool begin();

  // Initial stable body in the upper chamber, filled up from the neck, and a
  // first set of catchers below. Keeps the colour layers. Seeds the random
  // generator (everything after that is deterministic).
  void reset(uint32_t seed);

  // Starts a flow phase: steps counted from 0, release ramp between the two
  // step numbers (grains already below the neck count as released).
  void startFlow(uint32_t releaseStartStep, uint32_t releaseEndStep);
  void step();
  // Turns the glass by 180 degrees: the catchers dissolve, the (point-
  // symmetric) grid is reversed, a new random set of catchers is placed in
  // the new lower chamber.
  void flip();

  // Records the order in which grains cross the neck (rank per grain id), or
  // nullptr to stop. Used by the boot pre-run that assigns the colours.
  void trackArrivals(uint16_t* rankOut) { rankOut_ = rankOut; }
  // Colour layer of every grain from its arrival rank (first = layer 0);
  // nullptr: by grain id (fill order).
  void assignLayersFromRanks(const uint16_t* rank);

  // Geometry
  bool interior(int cx, int cy) const;
  int rowLeft(int cy) const { return left_[cy]; }    // inclusive
  int rowRight(int cy) const { return right_[cy]; }  // inclusive
  // Continuous inner half-width of the glass in cells at distance d (cells)
  // from the neck (shared with the painter for the drawn outline).
  static float profileHalfWidth(float d);

  // Cell access (cx 0..kW-1, cy 0..kH-1; out-of-range reads give kWall).
  uint16_t at(int cx, int cy) const;
  uint8_t layerOf(uint16_t cell) const { return layer_[cell - 1]; }
  uint8_t shadeOf(uint16_t cell) const;
  int velOf(uint16_t cell) const { return vy_[cell - 1]; }
  bool movedOf(uint16_t cell) const { return (flags_[cell - 1] & kMoved) != 0; }
  bool sparkOf(uint16_t cell) const { return (flags_[cell - 1] & kSparkMask) != 0; }
  // Catcher fade-in after placement, 0 (faint) .. 3 (full).
  uint8_t catcherFade() const;

  // Scenes. The hour (0..23, -1 = unknown) is used by the next placement;
  // forceScene (-1 = random) is for tests and previews.
  void setHour(int hour) { hour_ = static_cast<int8_t>(hour); }
  void forceScene(int scene) { forced_ = static_cast<int8_t>(scene); }
  Scene scene() const { return scene_; }
  // Magnet position in cells (valid while scene() == Magnet).
  float magnetX() const { return magX_; }
  float magnetY() const { return magY_; }
  uint16_t bucketTips() const { return bucketTips_; }

  // State
  uint32_t stepIndex() const { return stepIndex_; }
  uint16_t passed() const { return passed_; }
  uint16_t movingCount() const { return moving_; }
  uint16_t catcherCells() const { return catcherCells_; }
  uint32_t splashes() const { return splashes_; }
  // Every grain below the neck and none moving in the last step.
  bool drained() const { return passed_ == kGrains && moving_ == 0; }
  SandCounts counts() const;
  // Highest row of the lower chamber holding a grain (kH if none): pile height.
  int lowerPileTop() const;
  // Full check: every id exactly once, nothing inside a wall, catchers only
  // in the lower chamber. One pass over the grid: at flips or in tests.
  bool verify() const;

 private:
  static constexpr uint8_t kMoved = 0x01;
  static constexpr uint8_t kSparkOne = 0x10;
  static constexpr uint8_t kSparkMask = 0xF0;
  static constexpr int kMaxShapeCells = 2 * kBarMaxLen + 1;

  uint32_t rand32();
  int randRange(int lo, int hi);  // inclusive
  bool gateAllows(int fromRow, uint16_t cell);  // consumes budget when crossing
  bool verticalAllowed(int row, int dir, uint16_t cell);
  void updateGrain(int idx, int cx, int cy, uint16_t cell);
  void landed(uint16_t g, int row, int impact, uint8_t& f);
  int relativeImpact(int vy, uint16_t blocker) const;
  void moveCell(int from, int to, uint16_t cell);
  void clearCatchers();
  void placeScene();
  void placePins();
  void placeHour();
  bool placeShape(const int8_t* dx, const int8_t* dy, int n, int cx, int cy,
                  uint16_t value = kCatcher);
  void startMagnet();
  void updateMagnet();
  void applyMagnet(uint16_t g, int cx, int row);
  bool startBucket();
  void updateBucket();
  int rasterBucket(float angle, uint16_t* out, int max) const;
  bool setBucketAngle(float angle);
  int bucketLoad() const;

  uint16_t grid_[kCells];
  uint8_t left_[kH];
  uint8_t right_[kH];
  uint8_t layer_[kGrains];
  int8_t vx_[kGrains];
  int8_t vy_[kGrains];
  int8_t fx_[kGrains];  // sub-cell remainders, 1/16 cell
  int8_t fy_[kGrains];
  uint8_t flags_[kGrains];
  uint8_t stamp_[kGrains];  // last step tick that updated the grain
  uint16_t* rankOut_ = nullptr;

  uint32_t rng_ = 1;
  uint8_t tick_ = 0;
  uint32_t stepIndex_ = 0;
  uint32_t releaseStart_ = 0;
  uint32_t releaseEnd_ = 1;
  uint16_t passedAtStart_ = 0;
  uint16_t passed_ = 0;
  int16_t budget_ = 0;
  uint16_t moving_ = 0;
  uint8_t sparks_ = 0;
  uint16_t catcherAge_ = 0;
  uint16_t catcherCells_ = 0;
  uint32_t splashes_ = 0;

  // Scenes
  Scene scene_ = Scene::Pins;
  Scene lastScene_ = Scene::Pins;
  int8_t hour_ = -1;
  int8_t forced_ = -1;
  // Magnet (Lissajous): centre, amplitudes, angular steps, phase
  uint32_t sceneStep_ = 0;
  float magCx_ = 0, magCy_ = 0, magAx_ = 0, magAy_ = 0, magWx_ = 0, magWy_ = 0, magPh_ = 0;
  float magX_ = 0, magY_ = 0;
  // Tipping bucket
  enum class BucketState : uint8_t { Off, Upright, Tipping, Holding, Returning };
  static constexpr int kBucketMaxCells = 96;
  BucketState bucket_ = BucketState::Off;
  float bucketPx_ = 0, bucketPy_ = 0, bucketAngle_ = 0, bucketSpin_ = 0;
  int8_t bucketDir_ = 1;
  uint16_t bucketTimer_ = 0;
  uint16_t bucketTips_ = 0;
  uint16_t bucketCells_[kBucketMaxCells];
  uint8_t bucketCount_ = 0;
};

}  // namespace tf
