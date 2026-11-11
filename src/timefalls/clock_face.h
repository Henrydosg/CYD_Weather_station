// Upright text left of the hourglass (landscape, column of kColumnW px), top to
// bottom: weekday + date ("CHỦ NHẬT  08/11"), the time with a blinking colon
// (Roboto Bold 68, tight slots), a hairline cycle progress, the lunar date + note of the
// day (rain nowcast in cyan, else Tet / full moon...), the weather (icon,
// temperature, description, city; tap it, or anywhere below it, to open the
// weather station) and a small gear in the bottom-left corner (Settings). The
// clock always runs: no pause button; tapping the hourglass (right of the
// column) changes the sand palette.
// The time is drawn digit by digit through two small permanent sprites (a
// digit cell and the colon); the other parts use a sprite created for the
// draw and freed again, so the weather station's HTTPS requests keep their
// RAM. A draw that finds no RAM is retried by flush(). The hourglass region
// is right of the column, so a flip never covers the text.
#pragma once

#include <TFT_eSPI.h>
#include <stdint.h>

struct Icon;

namespace tf {

class ClockFace {
 public:
  explicit ClockFace(TFT_eSPI& tft) : tft_(tft), digitSpr_(&tft), colonSpr_(&tft), textSpr_(&tft) {}

  bool begin();  // false if the time sprites could not be allocated
  // Repaints the whole column (after the weather station had the screen).
  void redrawAll();
  void drawSplash();  // in the column, while the boot pre-run runs
  // "HH:MM" or nullptr when the time is unknown; colonOn = first half of a second.
  void setTime(const char* hhmm, bool colonOn);
  // Weekday + date, or the sync state (dim).
  void setDate(const char* text, bool dim);
  // Lunar date and the note of the day ("" = none); rain notes are cyan.
  void setNote(const char* lunar, const char* note, bool rain);
  // Weather: icon (nullptr = none yet), temperature ("" = no data: desc is then
  // shown dim as a placeholder), description and city.
  void setWeather(const Icon* icon, const char* temp, const char* desc, const char* city);
  // Diagnostics in place of the weather ('\n' between up to kDiagLines lines), nullptr = off.
  void setDiag(const char* text);
  void setProgress(float fraction);  // 0..1 of the cycle
  // Redraws parts whose sprite could not be allocated before.
  void flush();

  // Pixels covered by the column (no air motes there).
  static bool covers(int16_t x, int16_t y);

  // Touch zones (landscape screen pixels).
  static bool inTimeArea(int16_t x, int16_t y);
  static bool inWeatherButton(int16_t x, int16_t y);
  static bool inSettingsButton(int16_t x, int16_t y);
  static bool inHourglass(int16_t x, int16_t y);

  static constexpr int kDiagLines = 3;

 private:
  static constexpr int kTimeChars = 5;  // "HH:MM"

  void drawTime(bool force);
  void drawGlyph(int slot, char c, uint16_t ink);
  bool drawDate();
  bool drawNote();
  bool drawWeather();
  bool drawPart(int16_t y, int16_t h, void (ClockFace::*paint)(int16_t dy));
  void paintDate(int16_t dy);
  void paintNote(int16_t dy);
  void paintWeather(int16_t dy);
  void drawGear();

  TFT_eSPI& tft_;
  TFT_eSprite digitSpr_, colonSpr_, textSpr_;
  bool timeOk_ = false;
  bool datePending_ = false, notePending_ = false, weatherPending_ = false;

  char time_[8] = "";
  bool colonOn_ = true;
  char shown_[kTimeChars + 1] = "";  // glyphs on screen (0 = not drawn)
  uint16_t shownInk_ = 0;
  char date_[48] = "";
  bool dateDim_ = false;
  char lunar_[24] = "";
  char note_[48] = "";
  bool rain_ = false;
  const Icon* icon_ = nullptr;
  char temp_[12] = "";
  char desc_[40] = "";
  char city_[48] = "";
  char diag_[96] = "";
  bool diagOn_ = false;
  int16_t progressPx_ = -1;
};

}  // namespace tf
