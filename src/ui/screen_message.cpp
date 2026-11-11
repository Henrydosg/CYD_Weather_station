#include "screen_message.h"

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
// Header: envelope and title
constexpr int16_t kEnvX = ui::lay::X(12);
constexpr int16_t kEnvY = ui::lay::Y(9);
constexpr int16_t kEnvW = ui::lay::X(26);
constexpr int16_t kEnvH = ui::lay::Y(18);
constexpr int16_t kEnvStroke = ui::lay::S(2);
constexpr int16_t kTitleX = ui::lay::X(48);
constexpr int16_t kTitleMidY = ui::lay::Y(18);
constexpr int16_t kTitleMaxW = kW - kTitleX - 8;
constexpr int16_t kRuleX = ui::lay::X(6), kRuleY = ui::lay::Y(40);
// Text
constexpr int16_t kTextX = ui::lay::X(16), kTextY = ui::lay::Y(58), kTextMaxW = kW - 2 * kTextX;
constexpr int16_t kLineH = ui::lay::Y(21);
constexpr int kMaxLines = 6;
// OK button (hit area a bit larger than the pill)
constexpr int16_t kButtonW = ui::lay::X(140), kButtonH = ui::lay::Y(40);
constexpr int16_t kButtonX = (kW - kButtonW) / 2, kButtonY = ui::lay::Y(190);
constexpr int16_t kHitPad = ui::lay::S(12);

void drawEnvelope(TFT_eSPI& tft, uint16_t color) {
  for (int16_t i = 0; i < kEnvStroke; ++i) {
    tft.drawRect(kEnvX + i, kEnvY + i, kEnvW - 2 * i, kEnvH - 2 * i, color);
  }
  const int16_t foldX = kEnvX + kEnvW / 2, foldY = kEnvY + kEnvH * 3 / 5;
  tft.drawWideLine(kEnvX + 1, kEnvY + 1, foldX, foldY, kEnvStroke, color, kBackground);
  tft.drawWideLine(kEnvX + kEnvW - 2, kEnvY + 1, foldX, foldY, kEnvStroke, color, kBackground);
}

}  // namespace

void ScreenMessage::show(const RemoteMessage& m) {
  tft_.fillScreen(kBackground);
  drawEnvelope(tft_, kHighlight);
  char title[64];
  if (m.from[0] != '\0') {
    snprintf(title, sizeof(title), tr(Str::MessageFromFmt), m.from);
  } else {
    strlcpy(title, tr(Str::MessageTitle), sizeof(title));
  }
  tft_.setTextColor(kHighlight, kBackground);
  tft_.setTextDatum(ML_DATUM);
  ui::drawFitted(tft_, title, kTitleX, kTitleMidY, kTitleMaxW, kFontMedium, kFontSmall);
  tft_.drawFastHLine(kRuleX, kRuleY, kW - 2 * kRuleX, kLine);

  useFont(tft_, kFontSmall);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  ui::drawWrapped(tft_, m.text, kTextX, kTextY, kTextMaxW, kLineH, kMaxLines);

  ui::drawPill(tft_, kButtonX, kButtonY, kButtonW, kButtonH, tr(Str::MessageDone), kAccent,
               kOnAccent, false);
}

bool ScreenMessage::hitDone(int16_t x, int16_t y) const {
  return ui::hit(x, y, kButtonX - kHitPad, kButtonY - kHitPad, kButtonW + 2 * kHitPad,
                 kButtonH + 2 * kHitPad);
}
