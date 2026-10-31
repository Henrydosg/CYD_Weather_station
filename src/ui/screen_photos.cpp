#include "screen_photos.h"

#include <TJpg_Decoder.h>

#include "assets/fonts.h"
#include "config.h"
#include "draw_utils.h"
#include "strings.h"

using namespace theme;
using ui::useFont;

namespace {

constexpr int16_t kW = config::kScreenWidth;
constexpr int16_t kH = config::kScreenHeight;
constexpr int16_t kZoneW = kW / 3;
constexpr uint8_t kMaxJpgScale = 8;  // TJpgDec scales by 1, 2, 4 or 8

// Hint screen
constexpr int16_t kTitleMidY = 20;
constexpr int16_t kZoneTop = 44, kZoneBottom = 196;
constexpr int16_t kZoneMidY = (kZoneTop + kZoneBottom) / 2;
constexpr int16_t kChevronGap = 10;
constexpr int16_t kCountMidY = 218;
constexpr int16_t kMessageMidY = kH / 2 - 12, kMessageSubMidY = kH / 2 + 16;

TFT_eSPI* gTft = nullptr;

// TJpgDec output: one decoded block (up to 16 x 16 px) straight to the panel.
bool pushBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= kH) {
    return false;  // below the screen: stop decoding
  }
  gTft->pushImage(x, y, w, h, bitmap);
  return true;
}

}  // namespace

ScreenPhotos::ScreenPhotos(TFT_eSPI& tft) : tft_(tft) {}

void ScreenPhotos::open(uint32_t listVersion, uint32_t now) {
  phase_ = Phase::Scanning;
  openedVersion_ = listVersion;
  phaseAt_ = now;
  index_ = 0;
  drawMessage(tr(Str::PhotoLoading), kMuted);
}

void ScreenPhotos::update(const UiState& s, uint32_t now) {
  const PhotoList* list = s.photos;
  if (list == nullptr) {
    return;
  }
  switch (phase_) {
    case Phase::Scanning:
      if (list->version == openedVersion_) {
        return;  // main.cpp has not scanned yet
      }
      if (list->status == PhotoStatus::Ready) {
        drawHint(*list);
        phase_ = Phase::Hint;
        phaseAt_ = now;
      } else {
        char text[64];
        if (list->status == PhotoStatus::NoCard) {
          strlcpy(text, tr(Str::PhotoNoCard), sizeof(text));
        } else {
          snprintf(text, sizeof(text), tr(Str::PhotoNoFilesFmt), config::kPhotoDir);
        }
        drawMessage(text, kError);
        phase_ = Phase::Message;
      }
      break;
    case Phase::Hint:
      if (now - phaseAt_ >= config::kPhotoHintMs) {
        step(s, 0);
      }
      break;
    case Phase::Showing:
      if (list->count > 1 && now - phaseAt_ >= config::kPhotoSlideMs) {
        step(s, 1);
      }
      break;
    case Phase::Message:
      break;
  }
}

PhotoTarget ScreenPhotos::hitTest(int16_t x) const {
  if (phase_ != Phase::Hint && phase_ != Phase::Showing) {
    return PhotoTarget::Back;  // any tap leaves a message screen
  }
  if (x < kZoneW) {
    return PhotoTarget::Prev;
  }
  return x >= kW - kZoneW ? PhotoTarget::Next : PhotoTarget::Back;
}

void ScreenPhotos::step(const UiState& s, int8_t delta) {
  const PhotoList* list = s.photos;
  if (list == nullptr || list->status != PhotoStatus::Ready || list->count == 0) {
    return;
  }
  // Show the current photo first when leaving the hint.
  if (phase_ == Phase::Showing) {
    index_ = static_cast<uint8_t>((index_ + list->count + delta) % list->count);
  }
  phase_ = Phase::Showing;
  drawPhoto(list->paths[index_]);
  phaseAt_ = millis();  // decoding took a while
}

void ScreenPhotos::drawHint(const PhotoList& list) {
  tft_.fillScreen(kBackground);
  useFont(tft_, kFontMedium);
  tft_.setTextColor(kInk, kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.drawString(tr(Str::Photos), kW / 2, kTitleMidY);

  for (int i = 1; i < 3; ++i) {
    tft_.drawFastVLine(i * kZoneW, kZoneTop, kZoneBottom - kZoneTop, kLine);
  }
  useFont(tft_, kFontSmall);
  tft_.setTextColor(kAccent, kBackground);
  const int16_t prevX = kZoneW / 2, nextX = kW - kZoneW / 2;
  tft_.drawString(tr(Str::PhotoPrev), prevX, kZoneMidY);
  tft_.drawString(tr(Str::PhotoNext), nextX, kZoneMidY);
  const int16_t prevHalf = tft_.textWidth(tr(Str::PhotoPrev)) / 2;
  const int16_t nextHalf = tft_.textWidth(tr(Str::PhotoNext)) / 2;
  ui::drawChevron(tft_, prevX - prevHalf - kChevronGap, kZoneMidY, false, kAccent, kBackground);
  ui::drawChevron(tft_, nextX + nextHalf + kChevronGap, kZoneMidY, true, kAccent, kBackground);
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(tr(Str::PhotoBack), kW / 2, kZoneMidY);

  char buf[48];
  snprintf(buf, sizeof(buf), tr(Str::PhotoCountFmt), list.count,
           static_cast<unsigned long>(config::kPhotoSlideMs / 1000));
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(buf, kW / 2, kCountMidY);
}

void ScreenPhotos::drawMessage(const char* text, uint16_t color) {
  tft_.fillScreen(kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(color, kBackground);
  ui::drawFitted(tft_, text, kW / 2, kMessageMidY, kW - 20, kFontSmall, kFontTiny);
  useFont(tft_, kFontTiny);
  tft_.setTextColor(kMuted, kBackground);
  tft_.drawString(tr(Str::PhotoBack), kW / 2, kMessageSubMidY);
}

void ScreenPhotos::drawPhoto(const char* path) {
  gTft = &tft_;
  TJpgDec.setCallback(pushBlock);
  TJpgDec.setSwapBytes(true);

  uint16_t w = 0, h = 0;
  if (TJpgDec.getSdJpgSize(&w, &h, path) != JDR_OK || w == 0 || h == 0) {
    Serial.printf("[photos] cannot read %s\n", path);
    drawMessage(tr(Str::PhotoError), kError);
    return;
  }
  // Largest scale that fits the screen (bigger photos are reduced).
  uint8_t scale = 1;
  while (scale < kMaxJpgScale && (w / scale > kW || h / scale > kH)) {
    scale *= 2;
  }
  TJpgDec.setJpgScale(scale);
  const int16_t pw = w / scale, ph = h / scale;
  if (pw < kW || ph < kH) {
    tft_.fillScreen(kBackground);  // centred with borders
  }
  const uint32_t start = millis();
  TJpgDec.drawSdJpg((kW - pw) / 2, (kH - ph) / 2, path);
  Serial.printf("[photos] %s %ux%u /%u in %lu ms\n", path, w, h, scale,
                static_cast<unsigned long>(millis() - start));
}
