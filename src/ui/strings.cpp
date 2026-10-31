#include "strings.h"

#include <stddef.h>

#include "config.h"

namespace {

Language gLanguage = static_cast<Language>(config::kDefaultLanguage);

constexpr size_t kCount = static_cast<size_t>(Str::Count);

// Column order must match enum Str. UTF-8; every glyph must exist in
// tools/gen_fonts.py (TEXT_CHARS).
const char* const kVi[kCount] = {
    config::kLocationNameVi,
    "Đang chờ dữ liệu...",
    "Cảm giác",
    "Độ ẩm",
    "Gió",
    "Áp suất",
    "Chưa có dữ liệu",
    "Vừa cập nhật",
    "%lu phút trước",
    "%lu giờ trước",
    "Chạm để làm mới",
    "Đang tải...",
    "Lỗi cập nhật",
    "Đang đồng bộ giờ...",
    "%s, %02d/%02d",
    "Cài đặt",
    "Ngôn ngữ",
    "Hiệu chuẩn cảm ứng",
    "WiFi",
    "Chưa kết nối",
    "Chạm vào tâm dấu +",
    "Đã lưu hiệu chuẩn",
    "Chưa chính xác, thử lại",
};

const char* const kEn[kCount] = {
    config::kLocationNameEn,
    "Waiting for data...",
    "Feels like",
    "Humidity",
    "Wind",
    "Pressure",
    "No data yet",
    "Just updated",
    "%lu min ago",
    "%lu h ago",
    "Tap to refresh",
    "Updating...",
    "Update failed",
    "Syncing time...",
    "%s, %02d/%02d",
    "Settings",
    "Language",
    "Calibrate touch",
    "WiFi",
    "Not connected",
    "Tap the centre of the +",
    "Calibration saved",
    "Not accurate, try again",
};

const char* const kWeekdaysVi[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư",
                                   "Thứ Năm",  "Thứ Sáu", "Thứ Bảy"};
const char* const kWeekdaysEn[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                   "Thursday", "Friday", "Saturday"};
constexpr int kDaysPerWeek = 7;

}  // namespace

void setLanguage(Language lang) { gLanguage = lang; }

Language currentLanguage() { return gLanguage; }

const char* tr(Str id) {
  const size_t i = static_cast<size_t>(id);
  if (i >= kCount) {
    return "";
  }
  return gLanguage == Language::English ? kEn[i] : kVi[i];
}

const char* weekdayName(int wday) {
  if (wday < 0 || wday >= kDaysPerWeek) {
    return "";
  }
  return gLanguage == Language::English ? kWeekdaysEn[wday] : kWeekdaysVi[wday];
}
