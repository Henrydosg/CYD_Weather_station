// Template for local credentials.
// Copy this file to include/secrets.h and fill in your values.
// include/secrets.h is git-ignored and must never be committed.
#pragma once

namespace secrets {

constexpr char kWifiSsid[] = "YOUR_WIFI_SSID";
constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";

// https://home.openweathermap.org/api_keys
constexpr char kOwmApiKey[] = "YOUR_OPENWEATHERMAP_API_KEY";

}  // namespace secrets
