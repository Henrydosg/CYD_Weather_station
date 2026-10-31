#include "icon.h"

namespace {
constexpr uint16_t kMaxIconWidth = 128;
}

void drawIcon(TFT_eSPI& gfx, const Icon& icon, int16_t x, int16_t y, const theme::Palette& p,
              uint8_t glassAlpha) {
  if (icon.width > kMaxIconWidth) {
    return;
  }
  uint16_t line[kMaxIconWidth];

  // pushImage expects the colours in display byte order.
  const bool oldSwap = gfx.getSwapBytes();
  gfx.setSwapBytes(true);
  for (uint16_t row = 0; row < icon.height; ++row) {
    const uint16_t bg = theme::glassAt(p, y + row, glassAlpha);
    const uint32_t base = static_cast<uint32_t>(row) * icon.width;
    for (uint16_t col = 0; col < icon.width; ++col) {
      const uint8_t a = icon.alpha[base + col];
      line[col] = a == 0     ? bg
                : a == 255   ? icon.rgb[base + col]
                             : gfx.alphaBlend(a, icon.rgb[base + col], bg);
    }
    gfx.pushImage(x, y + row, icon.width, 1, line);
  }
  gfx.setSwapBytes(oldSwap);
}
