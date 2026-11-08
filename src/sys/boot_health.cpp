#include "boot_health.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <string.h>

namespace {

constexpr char kKeyBoots[] = "boots";
constexpr char kKeyCrashes[] = "crashes";
// Version being verified; still set after a rollback, so the older firmware
// learns which version failed.
constexpr char kKeyTrying[] = "trying";
constexpr char kKeyFailed[] = "failed";  // shown until an update succeeds
constexpr bool kReadOnly = true;

ResetKind classify(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:  return ResetKind::PowerOn;
    case ESP_RST_SW:       return ResetKind::Restart;
    case ESP_RST_PANIC:    return ResetKind::Crash;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return ResetKind::Watchdog;
    case ESP_RST_BROWNOUT: return ResetKind::Brownout;
    default:               return ResetKind::Other;
  }
}

const char* resetName(ResetKind kind) {
  switch (kind) {
    case ResetKind::PowerOn:  return "power on";
    case ResetKind::Restart:  return "restart";
    case ResetKind::Crash:    return "crash";
    case ResetKind::Watchdog: return "watchdog";
    case ResetKind::Brownout: return "brownout";
    case ResetKind::Other:    return "other";
  }
  return "?";
}

bool runningPendingVerify() {
  esp_ota_img_states_t state;
  return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

}  // namespace

// Overrides the weak C function of the Arduino core: it must not confirm a new
// firmware by itself at start-up; BootHealth::update() does.
extern "C" bool verifyRollbackLater() { return true; }

void BootHealth::begin() {
  report_.reset = classify(esp_reset_reason());
  report_.verifying = runningPendingVerify();

  Preferences prefs;
  if (!prefs.begin(config::kBootNamespace, !kReadOnly)) {
    Serial.println("[boot] NVS not available");
    return;
  }
  report_.boots = prefs.getULong(kKeyBoots, 0) + 1;
  prefs.putULong(kKeyBoots, report_.boots);
  report_.crashes = prefs.getULong(kKeyCrashes, 0);
  const ResetKind r = report_.reset;
  if (r == ResetKind::Crash || r == ResetKind::Watchdog || r == ResetKind::Brownout) {
    prefs.putULong(kKeyCrashes, ++report_.crashes);
  }

  char trying[config::kMaxVersionLen + 1] = "";
  prefs.getString(kKeyTrying, trying, sizeof(trying));
  if (report_.verifying) {
    prefs.putString(kKeyTrying, config::kFirmwareVersion);
  } else if (trying[0] != '\0') {
    // A newer version was being verified and this one runs instead: rolled back.
    // (A cable flash resets the OTA data, so there is no invalid slot then.)
    if (strcmp(trying, config::kFirmwareVersion) != 0 &&
        esp_ota_get_last_invalid_partition() != nullptr) {
      prefs.putString(kKeyFailed, trying);
    }
    prefs.remove(kKeyTrying);
  }
  prefs.getString(kKeyFailed, report_.failedVersion, sizeof(report_.failedVersion));
  prefs.end();

  Serial.printf("[boot] reset: %s, boots %lu, crashes %lu%s\n", resetName(r),
                static_cast<unsigned long>(report_.boots),
                static_cast<unsigned long>(report_.crashes),
                report_.verifying ? ", new firmware: verifying" : "");
  if (report_.failedVersion[0] != '\0') {
    Serial.printf("[boot] v%s failed after an update, rolled back\n", report_.failedVersion);
  }
}

void BootHealth::update(uint32_t now, bool healthy) {
  if (!report_.verifying) {
    return;
  }
  if (healthy) {
    esp_ota_mark_app_valid_cancel_rollback();
    report_.verifying = false;
    report_.failedVersion[0] = '\0';
    Preferences prefs;
    if (prefs.begin(config::kBootNamespace, !kReadOnly)) {
      prefs.remove(kKeyTrying);
      prefs.remove(kKeyFailed);
      prefs.end();
    }
    Serial.printf("[boot] v%s works, confirmed\n", config::kFirmwareVersion);
  } else if (now >= config::kOtaVerifyWindowMs) {  // millis() since boot
    Serial.printf("[boot] v%s not healthy after %lu s, rolling back\n",
                  config::kFirmwareVersion,
                  static_cast<unsigned long>(config::kOtaVerifyWindowMs / 1000));
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // Only returns when there is no previous firmware to go back to.
    Serial.println("[boot] no previous firmware, keeping this one");
    esp_ota_mark_app_valid_cancel_rollback();
    report_.verifying = false;
  }
}
