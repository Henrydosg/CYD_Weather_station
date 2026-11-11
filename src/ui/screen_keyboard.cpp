#include "screen_keyboard.h"

#include <ctype.h>

#include "assets/fonts.h"
#include "draw_utils.h"
#include "layout.h"
#include "strings.h"
#include "theme.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kTitleMaxW = ui::lay::X(280);

// Password field
constexpr int16_t kFieldX = ui::lay::X(6);
constexpr int16_t kFieldY = ui::lay::Y(34);
constexpr int16_t kFieldW = kW - 12;
constexpr int16_t kFieldH = ui::lay::Y(34);
constexpr int16_t kFieldRadius = ui::lay::S(10);
constexpr int16_t kFieldTextX = kFieldX + 10;
constexpr int16_t kFieldMidY = kFieldY + kFieldH / 2;
constexpr int16_t kToggleRightX = kFieldX + kFieldW - 10;
constexpr int16_t kToggleHitX = kFieldX + kFieldW - 60;
constexpr int16_t kFieldTextMaxW = kToggleHitX - kFieldTextX - 6;
constexpr int16_t kCaretW = ui::lay::X(2), kCaretPad = ui::lay::Y(7);
constexpr char kHiddenChar = '*';  // the font has no bullet glyph

// Keys: 10 per row, 29 x 36 px (touch targets are generous: gaps count too)
constexpr int16_t kKeyW = ui::lay::X(29);
constexpr int16_t kKeyH = ui::lay::Y(36);
constexpr int16_t kGap = ui::lay::S(3);
constexpr int16_t kKeyRadius = ui::lay::S(6);
constexpr int16_t kRowW = 10 * kKeyW + 9 * kGap;
constexpr int16_t kKeysX = (kW - kRowW) / 2;
constexpr int16_t kKeysY = ui::lay::Y(74);
constexpr int16_t kPitch = kKeyH + kGap;
constexpr int16_t kWideW = (kRowW - 7 * (kKeyW + kGap) - kGap) / 2;  // Shift / backspace
constexpr int16_t kSpaceW = ui::lay::X(118), kCancelW = ui::lay::X(48);
constexpr int kRows = 4;
constexpr uint32_t kPressFlashMs = 120;

const char* const kLower[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
const char* const kSymbols[] = {"1234567890", "@#$%&*-+()", "_!?/:;'\"."};

// Icon sizes are in base pixels (layout.h), scaled like the rest of the screen.
constexpr float s(float v) { return ui::lay::Sf(v); }

void drawShiftIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c, uint16_t bg) {
  constexpr float r = ui::lay::Sf(1.0f);
  tft.drawWedgeLine(cx, cy - s(8), cx - s(8), cy, r, r, c, bg);
  tft.drawWedgeLine(cx, cy - s(8), cx + s(8), cy, r, r, c, bg);
  tft.drawWedgeLine(cx - s(8), cy, cx - s(4), cy, r, r, c, bg);
  tft.drawWedgeLine(cx + s(8), cy, cx + s(4), cy, r, r, c, bg);
  tft.drawWedgeLine(cx - s(4), cy, cx - s(4), cy + s(7), r, r, c, bg);
  tft.drawWedgeLine(cx + s(4), cy, cx + s(4), cy + s(7), r, r, c, bg);
  tft.drawWedgeLine(cx - s(4), cy + s(7), cx + s(4), cy + s(7), r, r, c, bg);
}

void drawBackspaceIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c, uint16_t bg) {
  constexpr float r = ui::lay::Sf(1.0f);
  tft.drawWedgeLine(cx - s(10), cy, cx - s(4), cy - s(7), r, r, c, bg);
  tft.drawWedgeLine(cx - s(10), cy, cx - s(4), cy + s(7), r, r, c, bg);
  tft.drawWedgeLine(cx - s(4), cy - s(7), cx + s(10), cy - s(7), r, r, c, bg);
  tft.drawWedgeLine(cx - s(4), cy + s(7), cx + s(10), cy + s(7), r, r, c, bg);
  tft.drawWedgeLine(cx + s(10), cy - s(7), cx + s(10), cy + s(7), r, r, c, bg);
  tft.drawWedgeLine(cx - s(1), cy - s(3), cx + s(5), cy + s(3), r, r, c, bg);
  tft.drawWedgeLine(cx - s(1), cy + s(3), cx + s(5), cy - s(3), r, r, c, bg);
}

}  // namespace

void ScreenKeyboard::start(const char* title, KeyboardMode mode) {
  strlcpy(title_, title, sizeof(title_));
  mode_ = mode;
  clear();
  layout_ = Layout::Lower;
  visible_ = mode == KeyboardMode::Search;
  show();
}

void ScreenKeyboard::clear() { memset(text_, 0, sizeof(text_)); }

size_t ScreenKeyboard::maxLen() const {
  return mode_ == KeyboardMode::Password ? config::kMaxPasswordLen : config::kMaxSearchLen;
}

void ScreenKeyboard::show() {
  pressed_ = KeyRect{};
  tft_.fillScreen(kBackground);
  ui::drawBackHeader(tft_, title_, kTitleMaxW);
  drawField();
  drawKeys();
}

const char* ScreenKeyboard::row(int r) const {
  // Upper case is drawn from the lower-case rows (see drawKey).
  return layout_ == Layout::Symbols ? kSymbols[r] : kLower[r];
}

void ScreenKeyboard::drawField() {
  tft_.fillSmoothRoundRect(kFieldX, kFieldY, kFieldW, kFieldH, kFieldRadius, kAccent,
                           kBackground);
  tft_.fillSmoothRoundRect(kFieldX + 1, kFieldY + 1, kFieldW - 2, kFieldH - 2, kFieldRadius - 1,
                           kBackground, kAccent);

  // Show the end of a long password; the keyboard only types ASCII.
  char shown[config::kMaxPasswordLen + 1];
  const size_t len = strlen(text_);
  for (size_t i = 0; i < len; ++i) {
    shown[i] = visible_ ? text_[i] : kHiddenChar;
  }
  shown[len] = '\0';
  useFont(tft_, kFontMedium);
  const char* start = shown;
  while (*start != '\0' && tft_.textWidth(start) > kFieldTextMaxW) {
    ++start;
  }
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(ML_DATUM);
  const int16_t w = *start != '\0' ? tft_.drawString(start, kFieldTextX, kFieldMidY) : 0;
  tft_.fillRect(kFieldTextX + w + 1, kFieldY + kCaretPad, kCaretW, kFieldH - 2 * kCaretPad,
                kAccent);
  memset(shown, 0, sizeof(shown));

  if (mode_ == KeyboardMode::Password) {
    useFont(tft_, kFontSmall);
    tft_.setTextColor(kAccent, kBackground);
    tft_.setTextDatum(MR_DATUM);
    tft_.drawString(tr(visible_ ? Str::KeyHide : Str::KeyShow), kToggleRightX, kFieldMidY);
  }
}

void ScreenKeyboard::drawKeys() {
  tft_.fillRect(0, kKeysY, kW, config::kScreenHeight - kKeysY, kBackground);
  for (int r = 0; r < kRows; ++r) {
    // Walk the row key by key; past the last key keyAt() returns that key again.
    int16_t x = kKeysX;
    while (x < kKeysX + kRowW) {
      const KeyRect k = keyAt(x, kKeysY + r * kPitch + kKeyH / 2);
      if (k.key == Key::None || k.x + k.w <= x) {
        break;
      }
      drawKey(k, false);
      x = k.x + k.w + kGap;
    }
  }
}

void ScreenKeyboard::drawKey(const KeyRect& k, bool pressed) {
  const bool shiftOn = k.key == Key::Shift && layout_ == Layout::Upper;
  const bool accent = k.key == Key::Connect || shiftOn || pressed;
  const uint16_t bg = accent ? kAccent : kTrack;
  const uint16_t fg = accent ? kOnAccent : kInk;
  tft_.fillSmoothRoundRect(k.x, k.y, k.w, kKeyH, kKeyRadius, bg, kBackground);
  const int16_t cx = k.x + k.w / 2, cy = k.y + kKeyH / 2;

  tft_.setTextColor(fg, bg);
  tft_.setTextDatum(MC_DATUM);
  switch (k.key) {
    case Key::Char: {
      const char text[2] = {k.ch, '\0'};
      useFont(tft_, kFontMedium);
      tft_.drawString(text, cx, cy);
      break;
    }
    case Key::Shift:
      drawShiftIcon(tft_, cx, cy, fg, bg);
      break;
    case Key::Backspace:
      drawBackspaceIcon(tft_, cx, cy, fg, bg);
      break;
    case Key::Layout:
      useFont(tft_, kFontSmall);
      tft_.drawString(layout_ == Layout::Symbols ? "abc" : "123", cx, cy);
      break;
    case Key::Space:
      useFont(tft_, kFontSmall);
      tft_.drawString(tr(Str::KeySpace), cx, cy);
      break;
    case Key::Cancel:
      useFont(tft_, kFontSmall);
      tft_.drawString(tr(Str::KeyCancel), cx, cy);
      break;
    case Key::Connect:
      useFont(tft_, kFontSmall);
      tft_.drawString(tr(mode_ == KeyboardMode::Password ? Str::KeyConnect : Str::KeySearch),
                      cx, cy);
      break;
    default:
      break;
  }
}

ScreenKeyboard::KeyRect ScreenKeyboard::keyAt(int16_t x, int16_t y) const {
  KeyRect k;
  if (y < kKeysY || x < kKeysX || x >= kKeysX + kRowW) {
    return k;
  }
  const int r = (y - kKeysY) / kPitch;
  if (r >= kRows) {
    return k;
  }
  k.y = kKeysY + r * kPitch;
  const bool symbols = layout_ == Layout::Symbols;

  if (r == 3) {  // 123/abc, space, Cancel, Connect
    const int16_t spaceX = kKeysX + kWideW + kGap;
    const int16_t cancelX = spaceX + kSpaceW + kGap;
    const int16_t connectX = cancelX + kCancelW + kGap;
    if (x < spaceX) {
      k = {Key::Layout, 0, kKeysX, k.y, kWideW};
    } else if (x < cancelX) {
      k = {Key::Space, ' ', spaceX, k.y, kSpaceW};
    } else if (x < connectX) {
      k = {Key::Cancel, 0, cancelX, k.y, kCancelW};
    } else {
      k = {Key::Connect, 0, connectX, k.y, kKeysX + kRowW - connectX};
    }
    return k;
  }

  const char* chars = row(r);
  const int16_t count = strlen(chars);
  int16_t firstX = kKeysX + (10 - count) * (kKeyW + kGap) / 2;  // centred rows
  if (r == 2) {
    if (symbols) {
      firstX = kKeysX;  // 9 symbols, then backspace
    } else {
      firstX = kKeysX + kWideW + kGap;
      if (x < firstX) {
        return {Key::Shift, 0, kKeysX, k.y, kWideW};
      }
    }
    const int16_t backX = firstX + count * (kKeyW + kGap);
    if (x >= backX) {
      return {Key::Backspace, 0, backX, k.y, kKeysX + kRowW - backX};
    }
  }
  int i = (x - firstX) / (kKeyW + kGap);
  if (x < firstX) {
    i = 0;  // margin of a centred row counts as its first key
  }
  if (i >= count) {
    i = count - 1;
  }
  char ch = chars[i];
  if (layout_ == Layout::Upper) {
    ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
  }
  return {Key::Char, ch, static_cast<int16_t>(firstX + i * (kKeyW + kGap)), k.y, kKeyW};
}

KeyboardResult ScreenKeyboard::handleTap(int16_t x, int16_t y, uint32_t now) {
  if (ui::hit(x, y, 0, 0, ui::kBackHitW, ui::kBackHitH)) {
    return KeyboardResult::Cancel;
  }
  if (mode_ == KeyboardMode::Password &&
      ui::hit(x, y, kToggleHitX, kFieldY, kW - kToggleHitX, kFieldH)) {
    visible_ = !visible_;
    drawField();
    return KeyboardResult::None;
  }
  const KeyRect k = keyAt(x, y);
  size_t len = strlen(text_);
  switch (k.key) {
    case Key::None:
      return KeyboardResult::None;
    case Key::Cancel:
      return KeyboardResult::Cancel;
    case Key::Connect:
      return KeyboardResult::Submit;
    case Key::Shift:
      layout_ = layout_ == Layout::Upper ? Layout::Lower : Layout::Upper;
      drawKeys();
      return KeyboardResult::None;
    case Key::Layout:
      layout_ = layout_ == Layout::Symbols ? Layout::Lower : Layout::Symbols;
      drawKeys();
      return KeyboardResult::None;
    case Key::Backspace:
      if (len > 0) {
        text_[len - 1] = '\0';
      }
      break;
    default:  // Char, Space
      if (len < maxLen()) {
        text_[len] = k.ch;
        text_[len + 1] = '\0';
      }
      break;
  }
  drawField();
  // Short highlight so a resistive tap visibly registered.
  if (pressed_.key != Key::None) {
    drawKey(pressed_, false);
  }
  pressed_ = k;
  pressedAt_ = now;
  drawKey(k, true);
  return KeyboardResult::None;
}

void ScreenKeyboard::update(uint32_t now) {
  if (pressed_.key != Key::None && now - pressedAt_ >= kPressFlashMs) {
    drawKey(pressed_, false);
    pressed_ = KeyRect{};
  }
}
