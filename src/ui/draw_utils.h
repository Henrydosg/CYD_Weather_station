// Drawing helpers shared by all screens.
#pragma once

#include <TFT_eSPI.h>

#include "theme.h"

namespace ui {

constexpr int16_t kCardRadius = 10;
// Soft drop shadow: kShadowSpread rings around the card, shifted down.
constexpr int16_t kShadowSpread = 6;
constexpr int16_t kShadowOffsetY = 3;
constexpr uint8_t kShadowAlpha = 36;  // kInk over white, next to the card

// Loads a smooth font on the TFT unless it is already the active one.
// All screens must use this (not tft.loadFont) so the cache stays correct.
void useFont(TFT_eSPI& tft, const uint8_t* font);

// Colour of shadow ring `ring` (1 = next to the shape .. spread = faintest).
uint16_t shadowColor(TFT_eSPI& tft, int16_t ring, int16_t spread);

// White card with a thin anti-aliased outline and a soft shadow. Its inside
// is theme::kBackground, so text on a card uses that as background colour.
// The shadow reaches kShadowSpread + kShadowOffsetY px below the card, so
// neighbours may need repainting: drawShadow / drawCardBody draw the two parts
// separately (all shadows first, then the bodies).
void drawCard(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);
void drawShadow(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);
void drawCardBody(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);

// Anti-aliased chevron (">" when pointRight, "<" otherwise) centred on cx, cy.
void drawChevron(TFT_eSPI& tft, int16_t cx, int16_t cy, bool pointRight, uint16_t color,
                 uint16_t bg);

// Sub-screen header "< title" on a white background. The back target is
// (0, 0, kBackHitW, kBackHitH). A title too wide for maxWidth uses the small font.
constexpr int16_t kHeaderMidY = 18;
constexpr int16_t kBackHitW = 150, kBackHitH = 40;
void drawBackHeader(TFT_eSPI& tft, const char* title, int16_t maxWidth);

// Draws text with the current colour and datum: `large` font if it fits in
// maxWidth, else `small`, else trimmed with "..". Respects UTF-8.
void drawFitted(TFT_eSPI& tft, const char* text, int16_t x, int16_t y, int16_t maxWidth,
                const uint8_t* large, const uint8_t* small);

// Simple rectangle hit test.
inline bool hit(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

}  // namespace ui
