#include "clock_face.h"

#include <math.h>
#include <string.h>

#include "assets/tf_fonts.h"
#include "timefalls_config.h"
#include "ui/assets/fonts.h"
#include "ui/icon.h"
#include "ui/layout.h"

namespace tf {

namespace {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Colours (art direction): strong contrast for the time, the rest subdued.
constexpr uint16_t kBg = rgb(0x07, 0x11, 0x1D);
constexpr uint16_t kTimeInk = rgb(0xF3, 0xEB, 0xDD);
constexpr uint16_t kTimeUnsynced = rgb(0x4A, 0x55, 0x66);
constexpr uint16_t kColonOff = rgb(0x2A, 0x33, 0x40);  // blinking colon, dark half
constexpr uint16_t kDateInk = rgb(0xC7, 0xA7, 0x6B);
constexpr uint16_t kLunarInk = rgb(0x8A, 0x7A, 0x5C);
constexpr uint16_t kNoteInk = rgb(0x8A, 0x96, 0xA8);
constexpr uint16_t kRainInk = rgb(0x64, 0xE4, 0xFF);
constexpr uint16_t kDimInk = rgb(0x5A, 0x65, 0x76);
constexpr uint16_t kTempInk = rgb(0xF3, 0xEB, 0xDD);
constexpr uint16_t kDescInk = rgb(0x64, 0xE4, 0xFF);
constexpr uint16_t kCityInk = rgb(0x8A, 0x96, 0xA8);
constexpr uint16_t kTrack = rgb(0x14, 0x21, 0x31);
constexpr uint16_t kProgress = rgb(0x7D, 0x6A, 0x48);
constexpr uint16_t kGearInk = rgb(0x5A, 0x66, 0x78);

using ui::lay::S;
using ui::lay::X;
using ui::lay::Y;
static_assert(kScreenW == config::kScreenWidth && kScreenH == config::kScreenHeight,
              "timefalls_config.h and the board disagree on the screen size");

// Column layout (screen pixels, base 320 x 240 values scaled by ui/layout.h):
// positions with X / Y, sizes tied to the fonts and icons (sprite heights,
// baselines, digit slots) with S, like the fonts of the size class.
// Text parts are kPartW wide at kPadX.
constexpr int16_t kPadX = X(12);
constexpr int16_t kPartW = kColumnW - kPadX;  // 164 on the 2.8"
// Weekday + date
constexpr int16_t kDateY = Y(20), kDateH = S(18), kDateBaseline = S(14);
// Time: Roboto Bold 68 (91 on the 3.5"), digits 50 px tall with about 32 px of
// ink; the slots are narrower than the font's advance (39 / 19 px) so the digits
// sit tight.
constexpr int16_t kDigitW = S(36), kColonW = S(15), kTimeH = S(54), kTimeBaseline = S(52);
constexpr int16_t kTimeY = Y(48);
constexpr int16_t kSlotX[] = {kPadX, kPadX + kDigitW, kPadX + 2 * kDigitW,
                              kPadX + 2 * kDigitW + kColonW, kPadX + 3 * kDigitW + kColonW};
constexpr int kColonSlot = 2;
// Cycle progress
constexpr int16_t kProgressX0 = kPadX, kProgressY = Y(112), kProgressW = X(150);
// Lunar date + note
constexpr int16_t kNoteY = Y(120), kNoteH = S(18), kNoteBaseline = S(14), kNoteGap = X(8);
// Weather block (icon left, temperature + description, city under them)
constexpr int16_t kWeatherY = Y(144), kWeatherH = S(52);
constexpr int16_t kIconX = 0, kIconY = S(4);  // "small" icon, 30 px (40 on the 3.5")
constexpr int16_t kTextX = S(38), kTempBaseline = S(24), kCityBaseline = S(42);
constexpr int16_t kDescGap = X(8);
constexpr int16_t kPlaceholderBaseline = S(24);
constexpr int16_t kDiagFirstBaseline = S(12), kDiagPitch = S(14);
// The text parts are drawn through a temporary sprite in horizontal bands of
// at most this many bytes, so it never takes a large block from the heap
// (the network task needs ~40 KB in one piece for TLS).
constexpr int32_t kMaxPartSpriteBytes = 12 * 1024;
// Settings gear, bottom left: a ring with kGearTeeth teeth (anti-aliased shapes)
constexpr float kGearX = X(24), kGearY = Y(222);
constexpr float kGearBodyR = ui::lay::Sf(6.5f), kGearHoleR = ui::lay::Sf(2.5f);
constexpr float kGearToothIn = ui::lay::Sf(5.0f), kGearToothOut = ui::lay::Sf(9.0f);
constexpr float kGearToothW = ui::lay::Sf(3.0f);
constexpr int kGearTeeth = 8;
constexpr float kTwoPi = 6.2831853f;
// Touch zones: date, time and progress toggle the diagnostics; the gear
// (kGearTapW x the bottom kGearTapH px) opens Settings; the rest of the column
// below the time (lunar line, weather, the empty bottom) opens the weather station.
constexpr int16_t kTimeTapBottom = Y(116);
constexpr int16_t kGearTapW = X(52), kGearTapTop = Y(200);
// Splash, centred in the column
constexpr int16_t kSplashTitleDy = Y(-8), kSplashSubDy = Y(12);

// Trims a UTF-8 string until it fits maxW in the sprite's current font.
void trimToWidth(TFT_eSprite& spr, char* text, int16_t maxW) {
  for (size_t n = strlen(text); n > 0 && spr.textWidth(text) > maxW;) {
    do {
      --n;
    } while (n > 0 && (static_cast<uint8_t>(text[n]) & 0xC0) == 0x80);  // UTF-8 start byte
    text[n] = '\0';
  }
}

}  // namespace

bool ClockFace::begin() {
  timeOk_ = digitSpr_.createSprite(kDigitW, kTimeH) != nullptr &&
            colonSpr_.createSprite(kColonW, kTimeH) != nullptr;
  if (timeOk_) {
    digitSpr_.loadFont(kTfFontTime);
    colonSpr_.loadFont(kTfFontTime);
    // The glyph boxes (39 / 19 px) are wider than the tight slots: with wrapping
    // on, TFT_eSPI would move each glyph to the next line, out of the sprite.
    digitSpr_.setTextWrap(false, false);
    colonSpr_.setTextWrap(false, false);
  }
  return timeOk_;
}

void ClockFace::redrawAll() {
  tft_.fillRect(0, 0, kColumnW, kScreenH, kBg);
  drawTime(true);
  datePending_ = !drawDate();
  notePending_ = !drawNote();
  weatherPending_ = !drawWeather();
  drawGear();
  const int16_t px = progressPx_;
  progressPx_ = -1;
  setProgress(px > 0 ? static_cast<float>(px) / kProgressW : 0.0f);
}

void ClockFace::drawSplash() {
  tft_.loadFont(kFontLabel);
  tft_.setTextDatum(C_BASELINE);
  tft_.setTextColor(kTimeInk, kBg);
  const int16_t cx = kColumnW / 2, cy = kScreenH / 2;
  tft_.drawString("T I M E   F A L L S", cx, cy + kSplashTitleDy);
  tft_.setTextColor(kDateInk, kBg);
  tft_.drawString("SAND ATELIER", cx, cy + kSplashSubDy);
  tft_.unloadFont();
}

void ClockFace::setTime(const char* hhmm, bool colonOn) {
  const char* text = hhmm != nullptr ? hhmm : "--:--";
  if (strcmp(text, time_) == 0 && colonOn == colonOn_) {
    return;
  }
  colonOn_ = colonOn;
  strlcpy(time_, text, sizeof(time_));
  drawTime(false);
}

void ClockFace::setDate(const char* text, bool dim) {
  if (strcmp(text, date_) == 0 && dim == dateDim_) {
    return;
  }
  strlcpy(date_, text, sizeof(date_));
  dateDim_ = dim;
  datePending_ = !drawDate();
}

void ClockFace::setNote(const char* lunar, const char* note, bool rain) {
  if (strcmp(lunar, lunar_) == 0 && strcmp(note, note_) == 0 && rain == rain_) {
    return;
  }
  strlcpy(lunar_, lunar, sizeof(lunar_));
  strlcpy(note_, note, sizeof(note_));
  rain_ = rain;
  notePending_ = !drawNote();
}

void ClockFace::setWeather(const Icon* icon, const char* temp, const char* desc, const char* city) {
  if (icon == icon_ && strcmp(temp, temp_) == 0 && strcmp(desc, desc_) == 0 &&
      strcmp(city, city_) == 0) {
    return;
  }
  icon_ = icon;
  strlcpy(temp_, temp, sizeof(temp_));
  strlcpy(desc_, desc, sizeof(desc_));
  strlcpy(city_, city, sizeof(city_));
  if (!diagOn_) {
    weatherPending_ = !drawWeather();
  }
}

void ClockFace::setDiag(const char* text) {
  const bool on = text != nullptr;
  if (on == diagOn_ && (!on || strcmp(text, diag_) == 0)) {
    return;
  }
  diagOn_ = on;
  strlcpy(diag_, on ? text : "", sizeof(diag_));
  weatherPending_ = !drawWeather();
}

void ClockFace::flush() {
  if (datePending_) datePending_ = !drawDate();
  if (notePending_) notePending_ = !drawNote();
  if (weatherPending_) weatherPending_ = !drawWeather();
}

// Only the glyphs that changed are pushed (the colon blinks alone).
void ClockFace::drawTime(bool force) {
  if (!timeOk_) return;
  const bool synced = time_[0] != '-' && time_[0] != '\0';
  const uint16_t ink = synced ? kTimeInk : kTimeUnsynced;
  if (ink != shownInk_) force = true;
  shownInk_ = ink;
  for (int slot = 0; slot < kTimeChars; ++slot) {
    const char c = time_[slot];
    if (slot == kColonSlot) {
      drawGlyph(slot, ':', colonOn_ ? ink : kColonOff);
      continue;
    }
    if (force || c != shown_[slot]) {
      drawGlyph(slot, c, ink);
      shown_[slot] = c;
    }
  }
}

void ClockFace::drawGlyph(int slot, char c, uint16_t ink) {
  TFT_eSprite& spr = slot == kColonSlot ? colonSpr_ : digitSpr_;
  const int16_t w = slot == kColonSlot ? kColonW : kDigitW;
  const char text[2] = {c, '\0'};
  spr.fillSprite(kBg);
  spr.setTextColor(ink, kBg);
  spr.setTextDatum(C_BASELINE);
  spr.drawString(text, w / 2, kTimeBaseline);
  spr.pushSprite(kSlotX[slot], kTimeY);
}

// Paints a text part of height h at (kPadX, y) through textSpr_, band by band
// (kMaxPartSpriteBytes each); paint() draws the whole part shifted up by -dy, the
// sprite clips it. False if even one band could not be allocated (flush() retries).
bool ClockFace::drawPart(int16_t y, int16_t h, void (ClockFace::*paint)(int16_t dy)) {
  int16_t bandH = static_cast<int16_t>(kMaxPartSpriteBytes / (kPartW * 2));
  if (bandH > h) bandH = h;
  if (textSpr_.createSprite(kPartW, bandH) == nullptr) return false;
  for (int16_t top = 0; top < h; top += bandH) {
    const int16_t rows = h - top < bandH ? h - top : bandH;
    textSpr_.fillSprite(kBg);
    textSpr_.setTextDatum(L_BASELINE);
    (this->*paint)(static_cast<int16_t>(-top));
    textSpr_.pushSprite(kPadX, y + top, 0, 0, kPartW, rows);
  }
  textSpr_.unloadFont();
  textSpr_.deleteSprite();
  return true;
}

bool ClockFace::drawDate() { return drawPart(kDateY, kDateH, &ClockFace::paintDate); }
bool ClockFace::drawNote() { return drawPart(kNoteY, kNoteH, &ClockFace::paintNote); }
bool ClockFace::drawWeather() { return drawPart(kWeatherY, kWeatherH, &ClockFace::paintWeather); }

void ClockFace::paintDate(int16_t dy) {
  textSpr_.loadFont(kFontTinyBold);
  textSpr_.setTextColor(dateDim_ ? kDimInk : kDateInk, kBg);
  char text[sizeof(date_)];
  strlcpy(text, date_, sizeof(text));
  trimToWidth(textSpr_, text, kPartW);
  textSpr_.drawString(text, 0, kDateBaseline + dy);
}

void ClockFace::paintNote(int16_t dy) {
  textSpr_.loadFont(kFontLabel);
  int16_t x = 0;
  if (lunar_[0] != '\0') {
    textSpr_.setTextColor(kLunarInk, kBg);
    textSpr_.drawString(lunar_, x, kNoteBaseline + dy);
    x = static_cast<int16_t>(textSpr_.textWidth(lunar_) + kNoteGap);
  }
  if (note_[0] != '\0' && x < kPartW) {
    char text[sizeof(note_)];
    strlcpy(text, note_, sizeof(text));
    trimToWidth(textSpr_, text, static_cast<int16_t>(kPartW - x));
    textSpr_.setTextColor(rain_ ? kRainInk : kNoteInk, kBg);
    textSpr_.drawString(text, x, kNoteBaseline + dy);
  }
}

void ClockFace::paintWeather(int16_t dy) {
  if (diagOn_) {
    textSpr_.loadFont(kFontTiny);
    textSpr_.setTextColor(kDimInk, kBg);
    char buf[sizeof(diag_)];
    strlcpy(buf, diag_, sizeof(buf));
    char* rest = buf;
    for (int i = 0; i < kDiagLines && rest != nullptr; ++i) {
      char* nl = strchr(rest, '\n');
      if (nl != nullptr) *nl = '\0';
      textSpr_.drawString(rest, 0, kDiagFirstBaseline + i * kDiagPitch + dy);
      rest = nl != nullptr ? nl + 1 : nullptr;
    }
  } else if (temp_[0] == '\0') {
    textSpr_.loadFont(kFontTiny);  // no data yet: a dim placeholder (or nothing)
    textSpr_.setTextColor(kDimInk, kBg);
    textSpr_.drawString(desc_, 0, kPlaceholderBaseline + dy);
  } else {
    if (icon_ != nullptr) {
      drawIcon(textSpr_, *icon_, kIconX, kIconY + dy, kBg);
    }
    textSpr_.loadFont(kFontMedium);
    textSpr_.setTextColor(kTempInk, kBg);
    textSpr_.drawString(temp_, kTextX, kTempBaseline + dy);
    const int16_t descX = static_cast<int16_t>(kTextX + textSpr_.textWidth(temp_) + kDescGap);
    textSpr_.loadFont(kFontLabel);
    char text[sizeof(city_)];
    if (descX < kPartW) {
      strlcpy(text, desc_, sizeof(text));
      trimToWidth(textSpr_, text, static_cast<int16_t>(kPartW - descX));
      textSpr_.setTextColor(kDescInk, kBg);
      textSpr_.drawString(text, descX, kTempBaseline + dy);
    }
    textSpr_.loadFont(kFontTiny);
    strlcpy(text, city_, sizeof(text));
    trimToWidth(textSpr_, text, static_cast<int16_t>(kPartW - kTextX));
    textSpr_.setTextColor(kCityInk, kBg);
    textSpr_.drawString(text, kTextX, kCityBaseline + dy);
  }
}

void ClockFace::drawGear() {
  tft_.fillRect(0, kGearTapTop, kGearTapW, kScreenH - kGearTapTop, kBg);
  for (int i = 0; i < kGearTeeth; ++i) {
    const float a = kTwoPi * i / kGearTeeth;
    const float c = cosf(a), s = sinf(a);
    tft_.drawWedgeLine(kGearX + c * kGearToothIn, kGearY + s * kGearToothIn,
                       kGearX + c * kGearToothOut, kGearY + s * kGearToothOut, kGearToothW / 2,
                       kGearToothW / 2, kGearInk, kBg);
  }
  tft_.fillSmoothCircle(static_cast<int32_t>(kGearX), static_cast<int32_t>(kGearY),
                        static_cast<int32_t>(kGearBodyR), kGearInk, kBg);
  tft_.fillSmoothCircle(static_cast<int32_t>(kGearX), static_cast<int32_t>(kGearY),
                        static_cast<int32_t>(kGearHoleR), kBg, kGearInk);
}

void ClockFace::setProgress(float fraction) {
  if (fraction < 0.0f) fraction = 0.0f;
  if (fraction > 1.0f) fraction = 1.0f;
  const int16_t px = static_cast<int16_t>(fraction * kProgressW);
  if (px == progressPx_) return;
  if (progressPx_ < 0 || px < progressPx_) {
    tft_.drawFastHLine(kProgressX0, kProgressY, kProgressW, kTrack);
    progressPx_ = 0;
  }
  if (px > progressPx_) {
    tft_.drawFastHLine(kProgressX0 + progressPx_, kProgressY, px - progressPx_, kProgress);
  }
  progressPx_ = px;
}

bool ClockFace::covers(int16_t x, int16_t /*y*/) { return x < kColumnW; }

bool ClockFace::inTimeArea(int16_t x, int16_t y) { return x < kColumnW && y < kTimeTapBottom; }

bool ClockFace::inWeatherButton(int16_t x, int16_t y) {
  return x < kColumnW && y >= kTimeTapBottom && !inSettingsButton(x, y);
}

bool ClockFace::inSettingsButton(int16_t x, int16_t y) {
  return x < kGearTapW && y >= kGearTapTop;
}

bool ClockFace::inHourglass(int16_t x, int16_t /*y*/) { return x >= kColumnW; }

}  // namespace tf
