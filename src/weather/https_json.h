// HTTPS GET of an Open-Meteo JSON endpoint with certificate verification,
// parsed straight from the stream through an ArduinoJson filter.
#pragma once

#include <ArduinoJson.h>
#include <stddef.h>

// Synchronous, bounded by config::kHttpTimeoutMs. `tag` prefixes log lines.
bool httpsGetJson(const char* url, const JsonDocument& filter, JsonDocument& doc,
                  const char* tag);

// strlcpy that never cuts a multi-byte UTF-8 character in half.
void copyUtf8(char* dst, const char* src, size_t size);
