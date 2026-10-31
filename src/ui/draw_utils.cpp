#include "draw_utils.h"

#include "assets/fonts.h"

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

uint16_t shadowColor(TFT_eSPI& tft, int16_t ring, int16_t spread) {
  // Quadratic fall-off: darkest next to the card, invisible at `spread`.
  const int32_t k = spread + 1 - ring;
  const int32_t alpha = kShadowAlpha * k * k / ((spread + 1) * (spread + 1));
  return tft.alphaBlend(static_cast<uint8_t>(alpha), theme::kInk, theme::kBackground);
}

void drawShadow(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
  // Rounded rings from the outside in, each one shade darker, shifted down.
  uint16_t outside = theme::kBackground;
  for (int16_t i = kShadowSpread; i >= 1; --i) {
    const uint16_t c = shadowColor(tft, i, kShadowSpread);
    tft.fillSmoothRoundRect(x - i, y + kShadowOffsetY - i, w + 2 * i, h + 2 * i, kCardRadius + i,
                            c, outside);
    outside = c;
  }
}

void drawCardBody(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
  using namespace theme;
  tft.fillSmoothRoundRect(x, y, w, h, kCardRadius, kCardBorder,
                          shadowColor(tft, 1, kShadowSpread));
  tft.fillSmoothRoundRect(x + kBorder, y + kBorder, w - 2 * kBorder, h - 2 * kBorder,
                          kCardRadius - kBorder, kBackground, kCardBorder);
}

void drawCard(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
  drawShadow(tft, x, y, w, h);
  drawCardBody(tft, x, y, w, h);
}

namespace {

// Remove the last UTF-8 character from `s` (in place).
void popUtf8Char(char* s) {
  size_t len = strlen(s);
  while (len > 0) {
    const unsigned char c = static_cast<unsigned char>(s[--len]);
    s[len] = '\0';
    if ((c & 0xC0) != 0x80) {  // reached the lead byte
      break;
    }
  }
}

constexpr size_t kFitBufLen = 72;  // longest fitted text: weather description
constexpr int16_t kBackChevronX = 14;
constexpr int16_t kTitleX = 26;

}  // namespace

void drawFitted(TFT_eSPI& tft, const char* text, int16_t x, int16_t y, int16_t maxWidth,
                const uint8_t* large, const uint8_t* small) {
  useFont(tft, large);
  if (tft.textWidth(text) > maxWidth) {
    useFont(tft, small);
  }
  char buf[kFitBufLen];
  strlcpy(buf, text, sizeof(buf));
  if (tft.textWidth(buf) > maxWidth) {
    // Still too long: trim characters and append "..".
    do {
      popUtf8Char(buf);
      char probe[sizeof(buf)];
      snprintf(probe, sizeof(probe), "%s..", buf);
      if (tft.textWidth(probe) <= maxWidth || buf[0] == '\0') {
        strlcpy(buf, probe, sizeof(buf));
        break;
      }
    } while (true);
  }
  tft.drawString(buf, x, y);
}

void drawBackHeader(TFT_eSPI& tft, const char* title, int16_t maxWidth) {
  drawChevron(tft, kBackChevronX, kHeaderMidY, false, theme::kInk, theme::kBackground);
  tft.setTextColor(theme::kInk, theme::kBackground);
  tft.setTextDatum(ML_DATUM);
  drawFitted(tft, title, kTitleX, kHeaderMidY, maxWidth, kFontMedium, kFontSmall);
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
