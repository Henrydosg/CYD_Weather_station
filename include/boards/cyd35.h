// ESP32-3248S035R: 3.5" ST7796 480 x 320, XPT2046 resistive touch on the
// display's SPI bus (HSPI: SCLK 14, MOSI 13, MISO 12) with its own CS 33 and
// IRQ 36, backlight GPIO 27. Pins from the Sunton board definition
// (rzeldent/platformio-espressif32-sunton, esp32-3248S035R.json).
// Not yet checked on a real board: inversion, colour order, rotation mirroring
// and the default touch mapping may need adjusting.
#pragma once

#include <stdint.h>

namespace board {

constexpr char kName[] = "cyd35";
constexpr char kDescription[] = "ESP32-3248S035R 3.5\"";
// OTA manifest under docs/ota/ (tools/release.py writes it).
#define BOARD_OTA_MANIFEST "cyd35.json"

enum class Panel : uint8_t { Ili9341, St7796 };
constexpr Panel kPanel = Panel::St7796;
constexpr int16_t kScreenWidth = 480;  // landscape (rotation 1)
constexpr int16_t kScreenHeight = 320;
constexpr bool kInvertDisplay = false;

constexpr uint8_t kBacklightPin = 27;  // same as TFT_BL in platformio.ini

// XPT2046 shares the TFT's SPI bus: read through TFT_eSPI's SPIClass with a
// transaction (the bus lock keeps it apart from the display task).
constexpr bool kTouchSharesTftSpi = true;
constexpr uint8_t kTouchClk = 14;   // shared, unused when kTouchSharesTftSpi
constexpr uint8_t kTouchMosi = 13;  // shared
constexpr uint8_t kTouchMiso = 12;  // shared
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchIrq = 36;
constexpr int16_t kTouchRawLeft = 200;
constexpr int16_t kTouchRawRight = 3800;
constexpr int16_t kTouchRawTop = 300;
constexpr int16_t kTouchRawBottom = 3800;

constexpr uint8_t kLdrPin = 34;

}  // namespace board
