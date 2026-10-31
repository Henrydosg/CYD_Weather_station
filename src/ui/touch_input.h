// XPT2046 touch input on its own SPI bus (the TFT uses HSPI).
#pragma once

#include <SPI.h>
#include <XPT2046_Touchscreen.h>

class TouchInput {
 public:
  TouchInput();

  void begin();
  // Returns true once per new tap (debounced). Never blocks.
  bool tapped(uint32_t now);

 private:
  SPIClass spi_;
  XPT2046_Touchscreen ts_;
  bool wasTouched_ = false;
  uint32_t lastTap_ = 0;
};
