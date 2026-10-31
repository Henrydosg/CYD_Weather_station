// SD card on VSPI and the list of JPEG photos in config::kPhotoDir.
// The card is (re)mounted on every scan, so it can be swapped while running.
#pragma once

#include <stdint.h>

#include "config.h"

enum class PhotoStatus : uint8_t {
  NotScanned,
  NoCard,   // mount failed
  Empty,    // no .jpg / .jpeg files in the folder
  Ready,
};

// Plain data for the UI: full paths on the card, sorted by name.
struct PhotoList {
  PhotoStatus status = PhotoStatus::NotScanned;
  uint32_t version = 0;  // incremented by every scan
  uint8_t count = 0;
  char paths[config::kMaxPhotos][config::kMaxPhotoPathLen + 1] = {};
};

class PhotoLibrary {
 public:
  // Synchronous: mount + directory listing (a few ms to ~200 ms).
  void scan();
  const PhotoList& list() const { return list_; }

 private:
  PhotoList list_;
};
