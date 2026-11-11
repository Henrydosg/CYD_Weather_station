#include "draw_utils.h"

#include "assets/fonts.h"
#include "layout.h"

namespace ui {

namespace {
const uint8_t* gLoadedFont = nullptr;
constexpr int16_t kBorder = ui::lay::S(1);
constexpr int kBarCount = 4;
constexpr int16_t kBarW = ui::lay::X(3);
constexpr int16_t kBarGap = ui::lay::X(2);
constexpr int16_t kBarMinH = ui::lay::Y(4);
constexpr int16_t kBarStepH = ui::lay::Y(3);
constexpr int8_t kRssiLevels[kBarCount] = {-85, -75, -65, -55};  // dBm for 1..4 bars
constexpr float kChevronHalf = ui::lay::Sf(5.0f);   // half height
constexpr float kChevronDepth = ui::lay::Sf(4.0f);  // horizontal depth
constexpr float kChevronStroke = ui::lay::Sf(1.1f);
constexpr size_t kFitBufLen = 72;  // longest fitted text (city / network names)
constexpr size_t kWrapLineLen = 160;  // bytes of one wrapped line
constexpr int16_t kBackChevronX = ui::lay::X(14);
constexpr int16_t kTitleX = ui::lay::X(26);

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

void drawPill(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
              uint16_t fill, uint16_t fg, bool outline) {
  using namespace theme;
  tft.fillSmoothRoundRect(x, y, w, h, h / 2, fill, kBackground);
  uint16_t bg = fill;
  if (outline) {
    tft.fillSmoothRoundRect(x + kBorder, y + kBorder, w - 2 * kBorder, h - 2 * kBorder,
                            h / 2 - kBorder, kBackground, fill);
    bg = kBackground;
  }
  tft.setTextColor(fg, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2);
}

void drawSignalBars(TFT_eSPI& tft, int16_t x, int16_t bottom, uint8_t lit, uint16_t on) {
  for (int i = 0; i < kBarCount; ++i) {
    const int16_t h = kBarMinH + i * kBarStepH;
    tft.fillRect(x + i * (kBarW + kBarGap), bottom - h, kBarW, h, i < lit ? on : theme::kBarOff);
  }
}

uint8_t signalBars(int8_t rssi) {
  uint8_t bars = 1;  // connected always shows at least one bar
  for (int i = 1; i < kBarCount; ++i) {
    if (rssi >= kRssiLevels[i]) {
      bars = i + 1;
    }
  }
  return bars;
}

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

int drawWrapped(TFT_eSPI& tft, const char* text, int16_t x, int16_t y, int16_t maxWidth,
                int16_t lineH, int maxLines) {
  char line[kWrapLineLen] = "";
  char probe[kWrapLineLen];
  int lines = 0;
  const char* p = text;
  while (lines < maxLines) {
    // Next word (up to a space, a newline or the end)
    const char* end = p;
    while (*end != '\0' && *end != ' ' && *end != '\n') {
      ++end;
    }
    const size_t wordLen = static_cast<size_t>(end - p);
    if (line[0] == '\0') {
      snprintf(probe, sizeof(probe), "%.*s", static_cast<int>(wordLen), p);
    } else {
      snprintf(probe, sizeof(probe), "%s %.*s", line, static_cast<int>(wordLen), p);
    }
    const bool fits = tft.textWidth(probe) <= maxWidth || line[0] == '\0';
    if (fits) {
      strlcpy(line, probe, sizeof(line));
      p = end;
    }
    const bool lineDone = !fits || *p == '\n' || *p == '\0';
    if (lineDone) {
      const bool last = lines == maxLines - 1;
      const bool more = !fits || (*p != '\0' && p[1] != '\0');
      if (last && more) {
        // Out of lines: trim and append "..".
        do {
          snprintf(probe, sizeof(probe), "%s..", line);
          if (tft.textWidth(probe) <= maxWidth || line[0] == '\0') {
            break;
          }
          popUtf8Char(line);
        } while (true);
        strlcpy(line, probe, sizeof(line));
      }
      tft.drawString(line, x, y + lines * lineH);
      ++lines;
      line[0] = '\0';
      if (*p == '\0') {
        break;
      }
    }
    if (fits && (*p == ' ' || *p == '\n')) {
      ++p;
    }
  }
  return lines;
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
