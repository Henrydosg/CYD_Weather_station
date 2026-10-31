#include "draw_utils.h"

#include <math.h>

namespace ui {

namespace {
const uint8_t* gLoadedFont = nullptr;
constexpr float kChevronHalf = 5.0f;   // half height
constexpr float kChevronDepth = 4.0f;  // horizontal depth
constexpr float kChevronStroke = 1.1f;
}  // namespace

void useFont(TFT_eSPI& tft, const uint8_t* font) {
  if (font != gLoadedFont) {
    tft.loadFont(font);  // unloads the previous font
    gLoadedFont = font;
  }
}

void paintSky(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w, int16_t h) {
  for (int16_t row = y; row < y + h; ++row) {
    tft.drawFastHLine(x, row, w, theme::skyAt(p, row));
  }
}

void drawGlass(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w, int16_t h,
               int16_t radius, uint8_t alpha, bool rim, int16_t rowBegin, int16_t rowEnd) {
  if (rowEnd < 0 || rowEnd > h) {
    rowEnd = h;
  }
  const float r = radius;
  for (int16_t j = rowBegin; j < rowEnd; ++j) {
    const int16_t sy = y + j;
    const uint16_t sky = theme::skyAt(p, sy);
    const uint16_t glass = theme::blend(alpha, theme::kGlassTint, sky);
    const uint16_t lit = theme::blend(theme::kRimAlpha, theme::kText, glass);

    // Vertical distance from the corner circle centre (0 outside the corners).
    float dy = 0;
    if (j < radius) {
      dy = r - (j + 0.5f);
    } else if (j >= h - radius) {
      dy = (j + 0.5f) - (h - r);
    }
    // Anti-aliased corner pixels up to the first fully covered one.
    int16_t inset = 0;
    if (dy > 0) {
      const uint16_t edge = rim && j < radius ? lit : glass;
      for (int16_t i = 0; i < radius; ++i) {
        const float dx = r - (i + 0.5f);
        const float cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
        if (cover >= 1.0f) {
          break;
        }
        const uint16_t c =
            cover <= 0.0f ? sky : theme::blend(static_cast<uint8_t>(cover * 255), edge, sky);
        tft.drawPixel(x + i, sy, c);
        tft.drawPixel(x + w - 1 - i, sy, c);
        inset = i + 1;
      }
    }
    tft.drawFastHLine(x + inset, sy, w - 2 * inset, rim && j == 0 ? lit : glass);
  }
}

uint16_t drawCard(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w,
                  int16_t h) {
  drawGlass(tft, p, x, y, w, h, kCardRadius, theme::kGlassAlpha, true);
  return theme::glassAt(p, y + h / 2);
}

void drawChevron(TFT_eSPI& tft, int16_t cx, int16_t cy, bool pointRight, uint16_t color,
                 uint16_t bg) {
  const float dir = pointRight ? 1.0f : -1.0f;
  const float tipX = cx + dir * kChevronDepth / 2;
  const float tailX = cx - dir * kChevronDepth / 2;
  tft.drawWedgeLine(tailX, cy - kChevronHalf, tipX, cy, kChevronStroke, kChevronStroke, color, bg);
  tft.drawWedgeLine(tailX, cy + kChevronHalf, tipX, cy, kChevronStroke, kChevronStroke, color, bg);
}

}  // namespace ui
