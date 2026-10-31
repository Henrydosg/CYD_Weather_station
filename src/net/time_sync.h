// Real-time clock via SNTP. Non-blocking: SNTP runs in the background once
// started; this class only starts it and reports whether the time is valid.
#pragma once

#include <stdint.h>
#include <time.h>

class TimeSync {
 public:
  // Call every loop iteration. Starts SNTP on each new WiFi connection.
  void update(bool networkUp);

  // True once the system clock holds a real date.
  bool isValid() const;

  // Local broken-down time. Returns false while the clock is not set.
  bool localTime(struct tm& out) const;

 private:
  bool wasUp_ = false;
  bool reported_ = false;
};
