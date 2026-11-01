#include "remote_message.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../weather/https_json.h"

bool messageDue(const RemoteMessage& m, uint32_t seenId, uint32_t utcNow) {
  return m.id != 0 && m.id != seenId && utcNow >= m.showFromUtc &&
         (m.showUntilUtc == 0 || utcNow < m.showUntilUtc);
}

void MessageClient::update(uint32_t now, bool networkUp, bool busy) {
  if (networkUp && !wasUp_) {
    upSince_ = now;
  }
  wasUp_ = networkUp;
  if (!networkUp || busy) {
    return;
  }
  const bool firstDue = !checked_ && now - upSince_ >= config::kMessageFirstCheckMs;
  const bool due = checked_ && now - lastCheckAt_ >= config::kMessageCheckMs;
  if (!firstDue && !due) {
    return;
  }
  checked_ = true;
  lastCheckAt_ = now;
  fetch();  // on failure the old message stays; next try after the interval
}

bool MessageClient::fetch() {
  JsonDocument filter;
  for (const char* key : {"id", "from", "text", "show_from", "show_until"}) {
    filter[key] = true;
  }
  JsonDocument doc;
  if (!httpsGetJson(config::kMessageUrl, filter, doc, "message")) {
    return false;
  }
  RemoteMessage m{};
  m.id = doc["id"] | 0;
  copyUtf8(m.from, doc["from"] | "", sizeof(m.from));
  copyUtf8(m.text, doc["text"] | "", sizeof(m.text));
  m.showFromUtc = doc["show_from"] | 0;
  m.showUntilUtc = doc["show_until"] | 0;
  if (m.text[0] == '\0') {
    m.id = 0;
  }
  message_ = m;
  // The text itself is not logged (personal).
  Serial.printf("[message] id %lu, %u bytes\n", static_cast<unsigned long>(m.id),
                static_cast<unsigned>(strlen(m.text)));
  return true;
}
