// On-screen keyboard for a WiFi password: lower case, upper case (Shift) and
// digits/symbols (123) layouts, backspace, space, show/hide, Cancel/Connect.
#pragma once

#include <TFT_eSPI.h>

#include "config.h"

enum class KeyboardResult : uint8_t { None, Cancel, Submit };

class ScreenKeyboard {
 public:
  explicit ScreenKeyboard(TFT_eSPI& tft) : tft_(tft) {}

  // Starts with an empty password for network `ssid` (full redraw).
  void start(const char* ssid);
  // Full redraw keeping the typed password (after another screen was shown).
  void show();
  KeyboardResult handleTap(int16_t x, int16_t y, uint32_t now);
  // Releases the highlighted key after a short flash.
  void update(uint32_t now);

  const char* ssid() const { return ssid_; }
  const char* password() const { return password_; }
  // Wipes the typed password from RAM.
  void clear();

 private:
  enum class Layout : uint8_t { Lower, Upper, Symbols };
  enum class Key : uint8_t { None, Char, Shift, Backspace, Layout, Space, Cancel, Connect, Toggle };

  struct KeyRect {
    KeyRect() = default;
    KeyRect(Key k, char c, int kx, int ky, int kw)
        : key(k), ch(c), x(static_cast<int16_t>(kx)), y(static_cast<int16_t>(ky)),
          w(static_cast<int16_t>(kw)) {}

    Key key = Key::None;
    char ch = 0;
    int16_t x = 0, y = 0, w = 0;
  };

  void drawField();
  void drawKeys();
  void drawKey(const KeyRect& k, bool pressed);
  KeyRect keyAt(int16_t x, int16_t y) const;
  const char* row(int r) const;

  TFT_eSPI& tft_;
  char ssid_[config::kMaxSsidLen + 1] = "";
  char password_[config::kMaxPasswordLen + 1] = "";
  Layout layout_ = Layout::Lower;
  bool visible_ = false;  // show the password in clear text
  KeyRect pressed_;
  uint32_t pressedAt_ = 0;
};
