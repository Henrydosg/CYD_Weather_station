#include <string.h>

#include "settings.h"

namespace {

int indexOf(const SavedNetworks& nets, const char* ssid) {
  for (int i = 0; i < nets.count; ++i) {
    if (strcmp(nets.items[i].ssid, ssid) == 0) {
      return i;
    }
  }
  return -1;
}

void removeAt(SavedNetworks& nets, int index) {
  for (int i = index; i + 1 < nets.count; ++i) {
    nets.items[i] = nets.items[i + 1];
  }
  --nets.count;
}

}  // namespace

void rememberNetwork(SavedNetworks& nets, const char* ssid, const char* password) {
  const int existing = indexOf(nets, ssid);
  if (existing >= 0) {
    removeAt(nets, existing);
  } else if (nets.count == config::kMaxSavedNetworks) {
    --nets.count;  // drop the least recently used
  }
  for (int i = nets.count; i > 0; --i) {
    nets.items[i] = nets.items[i - 1];
  }
  strlcpy(nets.items[0].ssid, ssid, sizeof(nets.items[0].ssid));
  strlcpy(nets.items[0].password, password, sizeof(nets.items[0].password));
  ++nets.count;
}

bool forgetNetwork(SavedNetworks& nets, const char* ssid) {
  const int i = indexOf(nets, ssid);
  if (i < 0) {
    return false;
  }
  removeAt(nets, i);
  return true;
}

const WifiCredential* findNetwork(const SavedNetworks& nets, const char* ssid) {
  const int i = indexOf(nets, ssid);
  return i >= 0 ? &nets.items[i] : nullptr;
}
