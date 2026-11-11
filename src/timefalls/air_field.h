// Air outside the hourglass: faint motes carried by a smooth, divergence-free
// flow (the curl of a stream function made of a few travelling sine waves
// whose directions and speeds are drawn at boot). Swirls form, drift and
// dissolve without ever repeating exactly; a flip stirs the air (gust).
// Portable (host previews). Coordinates: scene-region pixels.
#pragma once

#include <stdint.h>

#include "timefalls_config.h"

namespace tf {

struct AirMote {
  float x = 0, y = 0;
  float age = 0, life = 1;  // seconds
  float speed = 0;          // last speed, px/s
  uint8_t hue = 0;
};

class AirField {
 public:
  void begin(uint32_t seed);
  // Advances the motes; t = animation time (s), dt = step (s).
  void update(float t, float dt);
  void gust(float t) { gustAt_ = t; gusting_ = true; }

  int count() const { return kAirMotes; }
  const AirMote& mote(int i) const { return motes_[i]; }
  // RGB565 of a mote (head) or of its one-pixel tail, blended on the background.
  uint16_t colour(int i, bool tail) const;

 private:
  struct Wave {
    float kx, ky, w, phase, amp;
  };
  float rand01();
  void spawn(AirMote& m);
  void velocity(float x, float y, float t, float& u, float& v) const;

  Wave waves_[kAirWaves];
  AirMote motes_[kAirMotes];
  uint32_t rng_ = 1;
  float gustAt_ = 0;
  bool gusting_ = false;
  float gain_ = 1.0f;
};

}  // namespace tf
