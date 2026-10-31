// Drawing helpers shared by all screens.
#pragma once

#include <TFT_eSPI.h>

#include "theme.h"

namespace ui {

constexpr int16_t kCardRadius = 10;

// Loads a smooth font on the TFT unless it is already the active one.
// All screens must use this (not tft.loadFont) so the cache stays correct.
void useFont(TFT_eSPI& tft, const uint8_t* font);

// Card with a thin anti-aliased outline. Its inside is theme::kBackground, so
// text on a card uses that as background colour.
void drawCard(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h);

// Pill button: filled with `fill`, or outlined in `fill` when `outline`.
// The label uses the current font; text colour is fg.
void drawPill(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
              uint16_t fill, uint16_t fg, bool outline);

// Four signal bars, bottom-aligned at `bottom`, `lit` of them in `on`.
void drawSignalBars(TFT_eSPI& tft, int16_t x, int16_t bottom, uint8_t lit, uint16_t on);
// 0..4 bars for an RSSI in dBm.
uint8_t signalBars(int8_t rssi);

// Anti-aliased chevron (">" when pointRight, "<" otherwise) centred on cx, cy.
void drawChevron(TFT_eSPI& tft, int16_t cx, int16_t cy, bool pointRight, uint16_t color,
                 uint16_t bg);

// Sub-screen header "< title" on the background. The back target is
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
