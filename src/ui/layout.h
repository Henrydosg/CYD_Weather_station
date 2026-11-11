// Screen layout for every supported resolution. The screens are designed in
// 320 x 240 ("base") pixels; each layout constant goes through one of these:
//   X(v)  horizontal position or width   (scaled by kScreenWidth / 320)
//   Y(v)  vertical position or height    (scaled by kScreenHeight / 240)
//   S(v)  size that must stay square: radius, icon, gap between glyphs,
//         stroke ... (scaled by the smaller of both factors)
// On the 2.8" board (320 x 240) all three are the identity, so its screens stay
// exactly as designed. On the 3.5" board (480 x 320: x 1.5, y 4/3, S 4/3) a
// value that looks wrong when scaled is overridden with ForBoard(base, cyd35).
#pragma once

#include <stdint.h>

#include "config.h"

namespace ui {
namespace lay {

constexpr int32_t kBaseW = 320;
constexpr int32_t kBaseH = 240;

// Rounded to the nearest pixel; negative values round away from zero too.
constexpr int32_t scaleRound(int32_t v, int32_t num, int32_t den) {
  return v >= 0 ? (v * num + den / 2) / den : -((-v * num + den / 2) / den);
}

// The square factor is the smaller one: compare W/320 with H/240.
constexpr bool kWidthLimits = static_cast<int32_t>(config::kScreenWidth) * kBaseH <=
                              static_cast<int32_t>(config::kScreenHeight) * kBaseW;
constexpr int32_t kSquareNum = kWidthLimits ? config::kScreenWidth : config::kScreenHeight;
constexpr int32_t kSquareDen = kWidthLimits ? kBaseW : kBaseH;

constexpr int16_t X(int32_t v) {
  return static_cast<int16_t>(scaleRound(v, config::kScreenWidth, kBaseW));
}
constexpr int16_t Y(int32_t v) {
  return static_cast<int16_t>(scaleRound(v, config::kScreenHeight, kBaseH));
}
constexpr int16_t S(int32_t v) {
  return static_cast<int16_t>(scaleRound(v, kSquareNum, kSquareDen));
}
// Same for float values (strokes, chevron sizes).
constexpr float Sf(float v) { return v * kSquareNum / kSquareDen; }

// True when the screen is the base resolution (2.8").
constexpr bool kIsBase = config::kScreenWidth == kBaseW && config::kScreenHeight == kBaseH;

// A hand-tuned value for the 3.5" board instead of the scaled one.
constexpr int16_t ForBoard(int16_t base, int16_t cyd35) { return kIsBase ? base : cyd35; }

static_assert(X(320) == config::kScreenWidth && Y(240) == config::kScreenHeight,
              "layout scale");

}  // namespace lay
}  // namespace ui
