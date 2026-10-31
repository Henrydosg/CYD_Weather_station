// Drawing helpers shared by all screens.
#pragma once

#include <TFT_eSPI.h>

#include "theme.h"

namespace ui {

constexpr int16_t kCardRadius = 10;

// Loads a smooth font on the TFT unless it is already the active one.
// All screens must use this (not tft.loadFont) so the cache stays correct.
void useFont(TFT_eSPI& tft, const uint8_t* font);

// White card with a thin anti-aliased outline. Its inside is
// theme::kBackground, so text on a card uses that as background colour.
void drawCard(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);

// Anti-aliased chevron (">" when pointRight, "<" otherwise) centred on cx, cy.
void drawChevron(TFT_eSPI& tft, int16_t cx, int16_t cy, bool pointRight, uint16_t color,
                 uint16_t bg);

// Simple rectangle hit test.
inline bool hit(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

}  // namespace ui
