// ESP32-2432S028R: 2.8" ILI9341 320 x 240, XPT2046 resistive touch on its own
// pins (bit-banged), backlight GPIO 21. Panel variant "TPM408-2.8" (single USB-C)
// needs display inversion.
#pragma once

#include <stdint.h>

namespace board {

constexpr char kName[] = "cyd28";
constexpr char kDescription[] = "ESP32-2432S028R 2.8\"";
// OTA manifest under docs/ota/ (tools/release.py writes it).
#define BOARD_OTA_MANIFEST "latest.json"

enum class Panel : uint8_t { Ili9341, St7796 };
constexpr Panel kPanel = Panel::Ili9341;
constexpr int16_t kScreenWidth = 320;  // landscape (rotation 1)
constexpr int16_t kScreenHeight = 240;
constexpr bool kInvertDisplay = true;

constexpr uint8_t kBacklightPin = 21;  // same as TFT_BL in platformio.ini

// XPT2046: own pins, bit-banged (the SD card keeps VSPI).
constexpr bool kTouchSharesTftSpi = false;
constexpr uint8_t kTouchClk = 25;
constexpr uint8_t kTouchMosi = 32;
constexpr uint8_t kTouchMiso = 39;
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchIrq = 36;
// Default raw->screen mapping until the user calibrates (raw value at screen
// x = 0 / right edge, y = 0 / bottom edge). Typical for these boards.
constexpr int16_t kTouchRawLeft = 200;
constexpr int16_t kTouchRawRight = 3700;
constexpr int16_t kTouchRawTop = 240;
constexpr int16_t kTouchRawBottom = 3800;

constexpr uint8_t kLdrPin = 34;

}  // namespace board
