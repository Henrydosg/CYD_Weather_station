// Remote message ("Lời nhắn"): a short note published on the project's GitHub
// Pages (config::kMessageUrl, written by tools/message.py) and shown full screen
// on the device until the user taps "OK". Checked every config::kMessageCheckMs.
#pragma once

#include <stdint.h>

#include "config.h"

// Plain data for the UI.
struct RemoteMessage {
  uint32_t id;            // 0 = no message published
  char from[config::kMaxMessageFromLen + 1];  // "" = no sender shown
  char text[config::kMaxMessageLen + 1];      // UTF-8, NFC
  uint32_t showFromUtc;   // 0 = at once
  uint32_t showUntilUtc;  // 0 = no end
};

// True when `m` is due at utcNow (inside its dates) and not yet closed.
bool messageDue(const RemoteMessage& m, uint32_t seenId, uint32_t utcNow);

class MessageClient {
 public:
  // busy: another HTTPS request is due in this loop iteration.
  void update(uint32_t now, bool networkUp, bool busy);
  const RemoteMessage& message() const { return message_; }

 private:
  bool fetch();

  RemoteMessage message_{};
  bool wasUp_ = false;
  uint32_t upSince_ = 0;
  bool checked_ = false;
  uint32_t lastCheckAt_ = 0;
};
