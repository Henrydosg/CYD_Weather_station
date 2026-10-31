// Drawing helpers shared by all screens.
#pragma once

#include <TFT_eSPI.h>

#include "theme.h"

namespace ui {

constexpr int16_t kCardRadius = 10;

// Loads a smooth font on the TFT unless it is already the active one.
// All screens must use this (not tft.loadFont) so the cache stays correct.
void useFont(TFT_eSPI& tft, const uint8_t* font);

// Paints the sky gradient behind a screen region, row by row.
void paintSky(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w, int16_t h);

// Dark glass rounded rectangle over the sky: every row is the sky of that row
// darkened by `alpha`, with anti-aliased corners. `rim` adds a light top edge.
// Only rows rowBegin..rowEnd-1 (relative to y) are drawn, so part of a card
// can be repainted; rowEnd < 0 means h.
void drawGlass(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w, int16_t h,
               int16_t radius, uint8_t alpha, bool rim, int16_t rowBegin = 0,
               int16_t rowEnd = -1);

// Standard glass card. Returns the glass colour of its middle row; text on a
// card uses theme::glassAt(palette, textMidY) as its background instead.
uint16_t drawCard(TFT_eSPI& tft, const theme::Palette& p, int16_t x, int16_t y, int16_t w,
                  int16_t h);

// Anti-aliased chevron (">" when pointRight, "<" otherwise) centred on cx, cy.
void drawChevron(TFT_eSPI& tft, int16_t cx, int16_t cy, bool pointRight, uint16_t color,
                 uint16_t bg);

// Simple rectangle hit test.
inline bool hit(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

}  // namespace ui
