#include "draw_utils.h"

namespace ui {

namespace {
const uint8_t* gLoadedFont = nullptr;
constexpr int16_t kBorder = 1;
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

void drawCard(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
  using namespace theme;
  tft.fillSmoothRoundRect(x, y, w, h, kCardRadius, kCardBorder, kBackground);
  tft.fillSmoothRoundRect(x + kBorder, y + kBorder, w - 2 * kBorder, h - 2 * kBorder,
                          kCardRadius - kBorder, kBackground, kCardBorder);
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
