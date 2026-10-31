// Photo viewer: JPEG files from the SD card, full screen, slideshow.
// Opens with a short hint of the tap zones: left third = previous, middle =
// back to the weather screen, right third = next; top corners cycle the panel gamma (left) and the
// photo tone correction (right), saved in NVS by main.cpp. Decoding a photo blocks the loop briefly
// (~0.3-0.5 s for 320 x 240), like an HTTP request.
#pragma once

#include <TFT_eSPI.h>

#include "ui_state.h"

enum class PhotoTarget : uint8_t { Back, Prev, Next, Gamma, Tone };

class ScreenPhotos {
 public:
  explicit ScreenPhotos(TFT_eSPI& tft);

  // Shows "Reading the SD card..." until a scan newer than `listVersion`.
  void open(uint32_t listVersion, uint32_t now);
  void update(const UiState& state, uint32_t now);
  // Top-left corner = panel gamma, top-right = photo tone, then thirds.
  PhotoTarget hitTest(int16_t x, int16_t y) const;
  // Previous / next photo (wraps around).
  void step(const UiState& state, int8_t delta);
  // Photo tone correction, 0 = off .. config::kPhotoToneLevels - 1.
  void setTone(uint8_t level);
  // Short label after a change (the tone also redraws the photo).
  void showGamma(PanelGamma mode);
  void showTone(const UiState& state, uint8_t level);

 private:
  enum class Phase : uint8_t { Scanning, Hint, Showing, Message };

  void drawHint(const PhotoList& list);
  void drawMessage(const char* text, uint16_t color);
  void drawPhoto(const char* path);
  void drawLabel(const char* text);

  TFT_eSPI& tft_;
  Phase phase_ = Phase::Scanning;
  uint32_t openedVersion_ = 0;
  uint32_t phaseAt_ = 0;  // hint shown / photo drawn
  uint8_t index_ = 0;
  bool labelShown_ = false;  // over the photo, removed after kPhotoLabelMs
  uint32_t labelAt_ = 0;
};
