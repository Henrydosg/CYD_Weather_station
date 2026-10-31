// Photo viewer: JPEG files from the SD card, full screen, slideshow.
// Opens with a short hint of the tap zones: left third = previous, middle =
// back, right third = next. Decoding a photo blocks the loop briefly
// (~0.3-0.5 s for 320 x 240), like an HTTP request.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class PhotoTarget : uint8_t { Back, Prev, Next };

class ScreenPhotos {
 public:
  explicit ScreenPhotos(TFT_eSPI& tft);

  // Shows "Reading the SD card..." until a scan newer than `listVersion`.
  void open(uint32_t listVersion, uint32_t now);
  void update(const UiState& state, uint32_t now);
  PhotoTarget hitTest(int16_t x) const;
  // Previous / next photo (wraps around).
  void step(const UiState& state, int8_t delta);

 private:
  enum class Phase : uint8_t { Scanning, Hint, Showing, Message };

  void drawHint(const PhotoList& list);
  void drawMessage(const char* text, uint16_t color);
  void drawPhoto(const char* path);

  TFT_eSPI& tft_;
  Phase phase_ = Phase::Scanning;
  uint32_t openedVersion_ = 0;
  uint32_t phaseAt_ = 0;  // hint shown / photo drawn
  uint8_t index_ = 0;
};
