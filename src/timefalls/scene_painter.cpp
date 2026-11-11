#include "scene_painter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace tf {

namespace {

struct Rgb {
  float r, g, b;
};

Rgb mix(Rgb a, Rgb b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Rgb scaled(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }

float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

float smoothstep(float e0, float e1, float x) {
  const float t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}

uint16_t to565(Rgb c) {
  const auto ch = [](float v) {
    v = v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v);
    return static_cast<uint32_t>(v + 0.5f);
  };
  return static_cast<uint16_t>(((ch(c.r) & 0xF8) << 8) | ((ch(c.g) & 0xFC) << 3) | (ch(c.b) >> 3));
}

Rgb hex(uint32_t v) {
  return {static_cast<float>((v >> 16) & 0xFF), static_cast<float>((v >> 8) & 0xFF),
          static_cast<float>(v & 0xFF)};
}

// --- Palette of the scene (art direction) ---------------------------------
const Rgb kBackground = hex(0x07111D);  // deep midnight
const Rgb kGlassTint = hex(0x0B1827);
const Rgb kGlassRim = hex(0x16273A);
const Rgb kIvory = hex(0xD3C6AA);       // glass outline, muted warm ivory
const Rgb kSheen = hex(0xEFE6D2);
const Rgb kGold = hex(0xC7A76B);        // champagne gold frame
const Rgb kGoldLight = hex(0xEAD5A4);
const Rgb kGoldDark = hex(0x6E5734);
const Rgb kSpark = hex(0xFFF4DC);
const Rgb kCatcherLight = hex(0xF1DFB2);  // brass pins and bars
const Rgb kCatcherBase = hex(0xC9A766);
const Rgb kCatcherDark = hex(0x7C6236);
const Rgb kMagnetGlow = hex(0xB9A6FF);    // the unseen magnet, a soft glint
constexpr float kGhostAlphaMin = 0.10f, kGhostAlphaMax = 0.30f;
constexpr float kMagnetCoreAlpha = 0.45f, kMagnetHaloAlpha = 0.18f;
constexpr float kMagnetCoreR2 = 0.8f, kMagnetHaloR2 = 2.6f;  // cells^2

// Sand layers, layer 0 = first to arrive (bottom of the lower pile).
struct Palette {
  const char* name;
  uint32_t layers[kLayers];
};
constexpr Palette kPaletteSet[kPalettes] = {
    // SUNSET (default): one warm gradient, so splashes blend instead of
    // speckling: deep purple at the bottom of the pile up to gold.
    // purple, violet, magenta, rose, coral, orange, amber, gold
    {"SUNSET", {0x5B3F8C, 0x7E4AA0, 0xA24F9C, 0xC9587F, 0xE56A5F, 0xF2873F, 0xF8A93A, 0xFCCB55}},
    // violet, turquoise, rose, pale green, coral, hot pink, amber, honey
    {"DUSK", {0x8A78B2, 0x3FC1C0, 0xC98A9B, 0xA8D38C, 0xE07A62, 0xF0609A, 0xDD9036, 0xEEC25B}},
    // plum, sage, clay, sky, sandstone, rosewood, wheat, cream
    {"DUNE", {0x6E5A7C, 0x8FA77F, 0xAE7868, 0x8DB3C9, 0xD39A69, 0xC2667A, 0xE5BE86, 0xF2DDB0}},
    // deep blue, lagoon, sea, mint, seafoam, sand, shell coral, sun
    {"TIDE", {0x4E6EA0, 0x2E9FB0, 0x6B9AB8, 0x7FD6B0, 0x9CC4C6, 0xE1CFA6, 0xEEA38A, 0xF4D35E}},
};
static_assert(kLayers == 8, "palettes hold 8 colours");
constexpr float kShadeGain[kShades] = {0.86f, 0.95f, 1.04f, 1.13f};
constexpr float kMovingGain = 1.16f;

// Static geometry in local pixels (centre of the assembly = origin).
// (all follow the grid size: |v| and |u| in pixels from the centre)
constexpr float kCapInner = kGridH;                     // |v| where the caps start
constexpr float kCapOuter = kAsmH / 2.0f;
constexpr float kCapHalfWidthPx = kAsmW / 2.0f - 1.0f;
constexpr float kCapRadius = 3.5f;
constexpr float kPostCentre = kBulbHalfW * kCellPx + 6.5f;  // just outside the bulb
constexpr float kPostHalf = 1.7f;
constexpr float kBeadCentre = kCapInner - 4.5f;
constexpr float kSheenFrom = 0.09f, kSheenFull = 0.30f;   // of kCapInner: the sheen
constexpr float kSheenFade = 0.82f, kSheenEnd = 0.98f;    // fades in, then out
constexpr float kCapEdgeShade = 9.0f;                     // px of darker cap ends
constexpr float kBeadRx = 3.0f, kBeadRy = 2.6f;
constexpr float kOutlinePx = 1.5f;
constexpr float kNeckCollarPx = 1.3f;  // extra glass thickness at the neck

float glassHalfPx(float av) {
  return kCellPx * SandSim::profileHalfWidth(av / kCellPx);
}

float roundedRectSdf(float px, float py, float hx, float hy, float r) {
  const float qx = fabsf(px) - (hx - r), qy = fabsf(py) - (hy - r);
  const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
  const float inside = qx > qy ? qx : qy;
  return sqrtf(ox * ox + oy * oy) + (inside < 0.0f ? inside : 0.0f) - r;
}

// Colour of one static pixel; (u, v) = pixel centre relative to the centre.
Rgb staticColour(float u, float v) {
  const float au = fabsf(u), av = fabsf(v);
  Rgb c = kBackground;

  // Posts: slim champagne columns with a bead near each end.
  if (av < kCapInner) {
    float cov = clamp01(0.5f - (fabsf(au - kPostCentre) - kPostHalf));
    const float bx = (au - kPostCentre) / kBeadRx, by = (av - kBeadCentre) / kBeadRy;
    const float bead = clamp01((1.0f - sqrtf(bx * bx + by * by)) * kBeadRy + 0.5f);
    cov = cov > bead ? cov : bead;
    if (cov > 0.0f) {
      const float s = clamp01(fabsf(au - kPostCentre) / (kPostHalf + 1.0f));
      const Rgb post = mix(kGoldLight, kGoldDark, s * s);
      c = mix(c, post, cov);
    }
  }

  // Glass: tinted inside, soft rim, point-symmetric sheen, ivory outline.
  if (av < kCapInner + 1.0f) {
    const float hw = glassHalfPx(av);
    const float slope = glassHalfPx(av + 0.5f) - glassHalfPx(av - 0.5f > 0 ? av - 0.5f : 0);
    const float e = (au - hw) / sqrtf(1.0f + slope * slope);  // + outside the inner surface
    const float band = kOutlinePx + kNeckCollarPx * (1.0f - smoothstep(3.0f, 9.0f, av));
    if (e < 0.0f && av < kCapInner) {
      Rgb g = mix(kGlassTint, kGlassRim, clamp01(1.0f + e / 3.0f) * 0.7f);
      const float us = (v < 0.0f ? -0.56f : 0.56f) * hw;
      const float d = (u - us) / 2.4f;
      const float fade = smoothstep(kSheenFrom * kCapInner, kSheenFull * kCapInner, av) *
                         (1.0f - smoothstep(kSheenFade * kCapInner, kSheenEnd * kCapInner, av));
      g = mix(g, kSheen, 0.085f * expf(-d * d) * fade);
      c = g;
    }
    const float cov = clamp01((e + 0.5f < band ? e + 0.5f : band) - (e - 0.5f > 0.0f ? e - 0.5f : 0.0f));
    if (cov > 0.0f) {
      c = mix(c, kIvory, cov * 0.88f);
    }
    // Faint thickness of the glass just outside the outline.
    const float halo = clamp01(1.0f - fabsf(e - band - 1.0f) / 1.5f) * 0.07f;
    if (halo > 0.0f && e > band) {
      c = mix(c, kIvory, halo);
    }
  }

  // Caps: rounded champagne plates, a light band and an engraved line.
  if (av >= kCapInner - 1.0f) {
    const float centre = (kCapInner + kCapOuter) / 2.0f;
    const float sd = roundedRectSdf(au, av - centre, kCapHalfWidthPx,
                                    (kCapOuter - kCapInner) / 2.0f, kCapRadius);
    const float cov = clamp01(0.5f - sd);
    if (cov > 0.0f) {
      const float t = clamp01((av - kCapInner) / (kCapOuter - kCapInner));
      Rgb plate = mix(kGoldDark, kGold, smoothstep(0.0f, 0.35f, t) * (1.0f - smoothstep(0.75f, 1.0f, t) * 0.6f));
      const float band = (t - 0.32f) / 0.13f;
      plate = mix(plate, kGoldLight, 0.55f * expf(-band * band));
      const float groove = (t - 0.74f) / 0.05f;
      plate = scaled(plate, 1.0f - 0.32f * expf(-groove * groove));
      plate = mix(plate, kGoldDark,
                  smoothstep(kCapHalfWidthPx - kCapEdgeShade, kCapHalfWidthPx, au) * 0.5f);
      c = mix(c, plate, cov);
    }
  }
  return c;
}

}  // namespace

const char* ScenePainter::paletteName(uint8_t index) {
  return kPaletteSet[index % kPalettes].name;
}


uint8_t ScenePainter::lutIndex(uint16_t colour) {
  for (uint8_t i = 0; i < lutCount_; ++i) {
    if (lut_[i] == colour) {
      return i;
    }
  }
  if (lutCount_ < 255) {
    lut_[lutCount_] = colour;
    return lutCount_++;
  }
  // Table full: nearest existing colour.
  const auto r = [](uint16_t c) { return static_cast<int>(c >> 11); };
  const auto g = [](uint16_t c) { return static_cast<int>((c >> 5) & 0x3F); };
  const auto b = [](uint16_t c) { return static_cast<int>(c & 0x1F); };
  int best = 0, bestD = 1 << 30;
  for (int i = 0; i < lutCount_; ++i) {
    const int dr = 2 * (r(lut_[i]) - r(colour)), dg = g(lut_[i]) - g(colour),
              db = 2 * (b(lut_[i]) - b(colour));
    const int d = dr * dr + dg * dg + db * db;
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return static_cast<uint8_t>(best);
}

void ScenePainter::buildStatic() {
  lutCount_ = 0;
  background_ = to565(kBackground);
  lutIndex(background_);
  const int total = kAsmW * kAsmH;
  // Half the pixels are computed, the other half mirrored: exact point symmetry.
  for (int i = 0; i < (total + 1) / 2; ++i) {
    const int x = i % kAsmW, y = i / kAsmW;
    const float u = x + 0.5f - kAsmW / 2.0f, v = y + 0.5f - kAsmH / 2.0f;
    const uint8_t idx = lutIndex(to565(staticColour(u, v)));
    static_[i] = idx;
    static_[total - 1 - i] = idx;
  }
}

bool ScenePainter::begin(const SandSim& sim) {
  (void)sim;
  if (static_ == nullptr) {
    static_ = static_cast<uint8_t*>(malloc(staticBytes()));
    if (static_ == nullptr) {
      return false;
    }
  }
  buildStatic();
  setPalette(palette_);
  return true;
}

void ScenePainter::setPalette(uint8_t index) {
  palette_ = static_cast<uint8_t>(index % kPalettes);
  const Palette& p = kPaletteSet[palette_];
  for (int layer = 0; layer < kLayers; ++layer) {
    const Rgb base = hex(p.layers[layer]);
    for (int shade = 0; shade < kShades; ++shade) {
      const Rgb c = scaled(base, kShadeGain[shade]);
      for (int state = 0; state < kStates; ++state) {
        uint16_t* k = keyColours_[1 + (layer * kShades + shade) * kStates + state];
        if (state == 0) {  // resting: lit top-left, shaded bottom-right
          k[0] = to565(mix(scaled(c, 1.07f), kSheen, 0.04f));
          k[1] = to565(c);
          k[2] = to565(scaled(c, 0.96f));
          k[3] = to565(scaled(c, 0.78f));
        } else if (state == 1) {  // moving: a little brighter
          const Rgb m = scaled(c, kMovingGain);
          k[0] = to565(mix(m, kSheen, 0.10f));
          k[1] = to565(m);
          k[2] = to565(m);
          k[3] = to565(scaled(m, 0.88f));
        } else {  // spark: brief light on impact
          k[0] = to565(mix(c, kSpark, 0.80f));
          k[1] = to565(mix(c, kSpark, 0.62f));
          k[2] = to565(mix(c, kSpark, 0.62f));
          k[3] = to565(mix(c, kSpark, 0.40f));
        }
      }
    }
    // Trail behind a fast grain: its colour, dimmed into the glass.
    uint16_t* t = keyColours_[kKeyTrailBase + layer];
    const Rgb trail = mix(base, kGlassTint, 0.58f);
    t[0] = t[1] = t[2] = t[3] = to565(trail);
  }
  // Catchers, fading in from the glass tint.
  for (int fade = 0; fade < kCatcherFades; ++fade) {
    const float a = (fade + 1.0f) / kCatcherFades;
    uint16_t* k = keyColours_[kKeyCatcherBase + fade];
    k[0] = to565(mix(kGlassTint, kCatcherLight, a));
    k[1] = to565(mix(kGlassTint, kCatcherBase, a));
    k[2] = to565(mix(kGlassTint, kCatcherBase, a));
    k[3] = to565(mix(kGlassTint, kCatcherDark, a));
    // Hour digits: faint glass that only sand on it makes readable.
    const float g = kGhostAlphaMin + (kGhostAlphaMax - kGhostAlphaMin) * fade / (kCatcherFades - 1.0f);
    uint16_t* h = keyColours_[kKeyGhostBase + fade];
    h[0] = to565(mix(kGlassTint, kSheen, g * 1.25f));
    h[1] = h[2] = to565(mix(kGlassTint, kIvory, g));
    h[3] = to565(mix(kGlassTint, kIvory, g * 0.7f));
  }
  uint16_t* core = keyColours_[kKeyMagnetBase];
  core[0] = core[1] = core[2] = core[3] = to565(mix(kGlassTint, kMagnetGlow, kMagnetCoreAlpha));
  uint16_t* halo = keyColours_[kKeyMagnetBase + 1];
  halo[0] = halo[1] = halo[2] = halo[3] = to565(mix(kGlassTint, kMagnetGlow, kMagnetHaloAlpha));
}

uint8_t ScenePainter::cellKey(const SandSim& sim, int cx, int cy, bool restOnly) const {
  const uint16_t cell = sim.at(cx, cy);
  if (cell == SandSim::kEmpty) {
    if (restOnly) {
      return 0;
    }
    const uint16_t below = sim.at(cx, cy + 1);
    if (SandSim::isGrain(below) && sim.velOf(below) >= kTrailVel) {
      return static_cast<uint8_t>(kKeyTrailBase + sim.layerOf(below));
    }
    if (sim.scene() == Scene::Magnet) {
      const float dx = cx + 0.5f - sim.magnetX(), dy = cy + 0.5f - sim.magnetY();
      const float d2 = dx * dx + dy * dy;
      if (d2 < kMagnetHaloR2) {
        return static_cast<uint8_t>(kKeyMagnetBase + (d2 < kMagnetCoreR2 ? 0 : 1));
      }
    }
    return 0;
  }
  if (cell == SandSim::kWall) {
    return 0;
  }
  if (cell == SandSim::kGhost) {
    return static_cast<uint8_t>(kKeyGhostBase + (restOnly ? kCatcherFades - 1 : sim.catcherFade()));
  }
  if (cell == SandSim::kCatcher) {
    return static_cast<uint8_t>(kKeyCatcherBase + (restOnly ? kCatcherFades - 1 : sim.catcherFade()));
  }
  int state = 0;
  if (!restOnly) {
    state = sim.sparkOf(cell) ? 2 : (sim.movedOf(cell) ? 1 : 0);
  }
  return static_cast<uint8_t>(1 + (sim.layerOf(cell) * kShades + sim.shadeOf(cell)) * kStates +
                              state);
}

void ScenePainter::computeKeys(const SandSim& sim, uint8_t* keys, bool restOnly) const {
  for (int cy = 0; cy < kGridH; ++cy) {
    for (int cx = 0; cx < kGridW; ++cx) {
      keys[cy * kGridW + cx] = sim.interior(cx, cy) ? cellKey(sim, cx, cy, restOnly) : 0;
    }
  }
}

void ScenePainter::fadeCatchers(uint8_t* keys, int fade) {
  for (size_t i = 0; i < kKeyBytes; ++i) {
    const int bases[2] = {kKeyCatcherBase, kKeyGhostBase};
    for (int base : bases) {
      if (keys[i] >= base && keys[i] < base + kCatcherFades) {
        keys[i] = static_cast<uint8_t>(fade < 0 ? 0 : base + fade);
      }
    }
  }
}

bool ScenePainter::isBackgroundPixel(int rx, int ry) const {
  const int sx = rx + kSceneX - kAsmX, sy = ry + kSceneTop - kAsmY;  // assembly, screen px
  if (sx < 0 || sy < 0) {
    return true;
  }
  const int lx = sx * kViewDen / kViewNum, ly = sy * kViewDen / kViewNum;
  if (lx >= kAsmW || ly >= kAsmH) {
    return true;
  }
  return static_[ly * kAsmW + lx] == 0;  // index 0 = background (first in the table)
}

uint16_t ScenePainter::samplePixel(const SandSim& sim, const uint8_t* keys, int ix, int iy,
                                   int toneFlip) const {
  const int px = ix - kInnerLeft, py = iy - kInnerTop;
  if (static_cast<unsigned>(px) < static_cast<unsigned>(kGridW * kCellPx) &&
      static_cast<unsigned>(py) < static_cast<unsigned>(kGridH * kCellPx)) {
    const int cx = px >> 1, cy = py >> 1;
    if (sim.interior(cx, cy)) {
      const uint8_t key = keys[cy * kGridW + cx];
      if (key != 0) {
        return keyColours_[key][(((py & 1) << 1) | (px & 1)) ^ toneFlip];
      }
    }
  }
  return lut_[static_[iy * kAsmW + ix]];
}

void ScenePainter::paintCells(const SandSim& sim, const uint8_t* keys, int cx0, int cy0,
                              int cx1, int cy1, uint16_t* out) const {
  const int w = cellsScreenPx(cx1 - cx0 + 1);
  const int h = cellsScreenPx(cy1 - cy0 + 1);
  const int ix0 = kInnerLeft + cx0 * kCellPx;
  const int iy0 = kInnerTop + cy0 * kCellPx;
  for (int y = 0; y < h; ++y) {
    uint16_t* row = out + y * w;
    const int iy = iy0 + y * kViewDen / kViewNum;  // nearest local pixel
    for (int x = 0; x < w; ++x) {
      row[x] = samplePixel(sim, keys, ix0 + x * kViewDen / kViewNum, iy, 0);
    }
  }
}

float ScenePainter::fitScale(float angle) {
  const float c = fabsf(cosf(angle)), s = fabsf(sinf(angle));
  const float ex = kAsmScreenW / 2.0f * c + kAsmScreenH / 2.0f * s;
  const float ey = kAsmScreenW / 2.0f * s + kAsmScreenH / 2.0f * c;
  float k = 1.0f;
  if (ex > kSceneW / 2.0f) {
    k = kSceneW / 2.0f / ex;
  }
  if (ey * k > kSceneH / 2.0f) {
    k = kSceneH / 2.0f / ey;
  }
  return k;
}

void ScenePainter::paintRegion(const SandSim& sim, const uint8_t* keys, float angle, float scale,
                               int row0, int rows, uint16_t* out) const {
  constexpr float kOne = 65536.0f;
  const float c = cosf(angle), s = sinf(angle);
  const float inv = 1.0f / (scale * kView);  // screen -> local pixels
  // Grains keep their light from above: past 90 degrees the 2 x 2 tone
  // pattern is mirrored, so the 180 degree frame equals the remapped one.
  const int toneFlip = c < 0.0f ? 3 : 0;
  // Inverse mapping: local = R(-angle) * (screen - pivot) / scale + centre.
  const int32_t dux = static_cast<int32_t>(lrintf(c * inv * kOne));
  const int32_t dvx = static_cast<int32_t>(lrintf(-s * inv * kOne));
  for (int r = 0; r < rows; ++r) {
    const float dy = (row0 + r) + 0.5f - kSceneH / 2.0f;
    const float dx = 0.5f - kSceneW / 2.0f;
    int32_t fu = static_cast<int32_t>(lrintf(((c * dx + s * dy) * inv + kAsmW / 2.0f) * kOne));
    int32_t fv = static_cast<int32_t>(lrintf(((-s * dx + c * dy) * inv + kAsmH / 2.0f) * kOne));
    uint16_t* line = out + r * kSceneW;
    for (int x = 0; x < kSceneW; ++x, fu += dux, fv += dvx) {
      const int ix = fu >> 16, iy = fv >> 16;
      if (static_cast<unsigned>(ix) < static_cast<unsigned>(kAsmW) &&
          static_cast<unsigned>(iy) < static_cast<unsigned>(kAsmH)) {
        line[x] = samplePixel(sim, keys, ix, iy, toneFlip);
      } else {
        line[x] = background_;
      }
    }
  }
}

}  // namespace tf
