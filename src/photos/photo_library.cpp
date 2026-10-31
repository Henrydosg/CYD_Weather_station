#include "photo_library.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace {

SPIClass sdSpi(VSPI);

bool isJpeg(const char* name) {
  if (name[0] == '.') {
    return false;  // hidden files, e.g. macOS "._photo.jpg"
  }
  const char* dot = strrchr(name, '.');
  return dot != nullptr && (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0);
}

int comparePaths(const void* a, const void* b) {
  return strcasecmp(static_cast<const char*>(a), static_cast<const char*>(b));
}

}  // namespace

void PhotoLibrary::scan() {
  ++list_.version;
  list_.count = 0;

  SD.end();  // forget a card that was removed or swapped
  sdSpi.begin(config::kSdSck, config::kSdMiso, config::kSdMosi, config::kSdCs);
  if (!SD.begin(config::kSdCs, sdSpi, config::kSdFrequency)) {
    list_.status = PhotoStatus::NoCard;
    Serial.println("[photos] no SD card");
    return;
  }

  File dir = SD.open(config::kPhotoDir);
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f && list_.count < config::kMaxPhotos;
         f = dir.openNextFile()) {
      const char* name = f.name();  // base name on Arduino-ESP32 2.x
      if (!f.isDirectory() && isJpeg(name)) {
        char* path = list_.paths[list_.count];
        const int len = snprintf(path, config::kMaxPhotoPathLen + 1, "%s/%s",
                                 config::kPhotoDir, name);
        if (len <= config::kMaxPhotoPathLen) {
          ++list_.count;
        }
      }
      f.close();
    }
    dir.close();
  }
  qsort(list_.paths, list_.count, sizeof(list_.paths[0]), comparePaths);
  list_.status = list_.count > 0 ? PhotoStatus::Ready : PhotoStatus::Empty;
  Serial.printf("[photos] %u photo(s) in %s\n", list_.count, config::kPhotoDir);
}
