#include "draw_utils.h"

namespace ui {

namespace {
const uint8_t* gLoadedFont = nullptr;
constexpr int16_t kCardRadius = 10;
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

uint16_t drawCard(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w,
                  int16_t h) {
  paintSky(tft, p, x - 1, y - 1, w + 2, h + 2);
  const uint16_t sky = theme::skyAt(p, y + h / 2);
  const uint16_t card = theme::blend(theme::kCardAlpha, theme::kText, sky);
  tft.fillSmoothRoundRect(x, y, w, h, kCardRadius, card, sky);
  return card;
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
