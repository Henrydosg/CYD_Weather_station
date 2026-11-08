#include "strings.h"

#include <stddef.h>

#include "config.h"

namespace {

Language gLanguage = static_cast<Language>(config::kDefaultLanguage);

struct Text {
  const char* vi;
  const char* en;
};

const char* pick(const Text& t) { return gLanguage == Language::English ? t.en : t.vi; }

// One entry per Str id, in enum order.
const Text kTexts[] = {
    {"Đang chờ dữ liệu...", "Waiting for data..."},
    {"Cảm giác %ld°", "Feels like %ld°"},
    {"Mặt trời mọc", "Sunrise"},
    {"Mặt trời lặn", "Sunset"},
    {"Độ ẩm", "Humidity"},
    {"Gió", "Wind"},
    {"Áp suất", "Pressure"},
    {"Mây", "Clouds"},
    {"Chỉ số UV", "UV index"},
    {"AQI", "AQI"},
    {"Nhiệt độ 24 giờ tới", "Next 24 hours"},
    {"Khả năng mưa", "Chance of rain"},
    {"%uh", "%uh"},
    {"Cài đặt", "Settings"},
    {"VI", "VI"},
    {"EN", "EN"},
    {"Độ sáng", "Brightness"},
    {"Tự động", "Auto"},
    {"Cao", "High"},
    {"Vừa", "Medium"},
    {"Thấp", "Low"},
    {"Tự động %u%%", "Auto %u%%"},
    {"Hiệu chuẩn cảm ứng", "Calibrate touch"},
    {"WiFi", "WiFi"},
    {"Chưa kết nối", "Not connected"},
    {"Thành phố", "Cities"},
    {"Đổi thành phố sau", "Next city after"},
    {"%u giây", "%u s"},
    {"Chạm vào tâm dấu +", "Tap the centre of the +"},
    {"Đã lưu hiệu chuẩn", "Calibration saved"},
    {"Chưa chính xác, thử lại", "Not accurate, try again"},
    {"Quét lại", "Rescan"},
    {"Đang quét...", "Scanning..."},
    {"Quét thất bại", "Scan failed"},
    {"Không tìm thấy mạng", "No networks found"},
    {"Đã kết nối: %s", "Connected: %s"},
    {"Đang kết nối: %s", "Connecting: %s"},
    {"Đã lưu", "Saved"},
    {"Trước", "Prev"},
    {"Sau", "Next"},
    {"Đang kết nối...", "Connecting..."},
    {"Đã kết nối", "Connected"},
    {"Sai mật khẩu hoặc", "Wrong password or"},
    {"không kết nối được", "network unreachable"},
    {"Nhập lại", "Try again"},
    {"Quên mạng", "Forget"},
    {"dấu cách", "space"},
    {"Huỷ", "Cancel"},
    {"Kết nối", "Connect"},
    {"Tìm", "Search"},
    {"Hiện", "Show"},
    {"Ẩn", "Hide"},
    {"+ Thêm", "+ Add"},
    {"Thêm thành phố", "Add city"},
    {"Kết quả: %s", "Results: %s"},
    {"Đang tìm...", "Searching..."},
    {"Không tìm được, kiểm tra WiFi", "Search failed, check WiFi"},
    {"Không tìm thấy", "Nothing found"},
    {"Lên", "Up"},
    {"Xuống", "Down"},
    {"Xoá", "Remove"},
    {"Đóng", "Close"},
    {"%.1f tr. dân", "%.1fM people"},
    {"%.0f nghìn dân", "%.0fk people"},
    {"%lu dân", "%lu people"},
    {"Ảnh", "Photos"},
    {"Trước", "Previous"},
    {"Sau", "Next"},
    {"Về thời tiết", "Weather"},
    {"%u ảnh, tự chuyển sau %lu giây", "%u photos, next every %lu s"},
    {"Đang đọc thẻ nhớ...", "Reading the SD card..."},
    {"Không thấy thẻ nhớ", "No SD card found"},
    {"Không có ảnh .jpg trong %s", "No .jpg photos in %s"},
    {"Không đọc được ảnh", "Cannot read this photo"},
    {"Firmware", "Firmware"},
    {"Phiên bản v%s", "Version v%s"},
    {"Chạm Kiểm tra để tìm bản mới", "Tap Check to look for updates"},
    {"Đang kiểm tra...", "Checking..."},
    {"Đã là bản mới nhất", "Up to date"},
    {"Có bản mới v%s", "New version v%s"},
    {"Đang tải %u%%", "Downloading %u%%"},
    {"Không cập nhật được, thử lại sau", "Update failed, try again later"},
    {"Xong, đang khởi động lại...", "Done, restarting..."},
    {"Chưa có WiFi", "No WiFi"},
    {"Không rút điện khi đang cập nhật", "Keep the power on while updating"},
    {"Kiểm tra", "Check"},
    {"Cập nhật", "Update"},
    {"ÂL %u/%u%s", "Lunar %u/%u%s"},
    {" nhuận", " leap"},
    {"Còn %d ngày đến Tết", "Tet in %d days"},
    {"Chúc mừng năm mới!", "Happy Lunar New Year!"},
    {"Hôm nay rằm tháng %u", "Full moon, lunar month %u"},
    {"Hôm nay mùng 1 tháng %u", "Lunar month %u begins"},
    {"Mưa sau ~%u phút", "Rain in ~%u min"},
    {"Mưa lúc ~%02d:%02d", "Rain at ~%02d:%02d"},
    {"Đang mưa, tạnh ~%02d:%02d", "Raining, stops ~%02d:%02d"},
    {"Mưa còn kéo dài", "Rain continues"},
    {"Lời nhắn từ %s", "Message from %s"},
    {"Lời nhắn", "Message"},
    {"Đã đọc", "OK"},
    {"Bản v%s bị lỗi, đã quay lại", "v%s failed, rolled back"},
    {"Máy tự quay lại bản trước để vẫn chạy được", "The previous version was restored"},
    {"Thông tin", "Info"},
    {"Thông tin máy", "Device info"},
    {"Phiên bản", "Version"},
    {"v%s  đã xác nhận", "v%s  confirmed"},
    {"v%s  đang kiểm tra (%u phút)", "v%s  verifying (%u min)"},
    {"Đang chạy", "Uptime"},
    {"%lu ngày %02u:%02u", "%lu d %02u:%02u"},
    {"Khởi động", "Boots"},
    {"%lu lần, lỗi %lu lần", "%lu, crashes %lu"},
    {"Lần cuối do", "Last reset"},
    {"Rút điện / bật lại", "Power on"},
    {"Khởi động lại", "Restart"},
    {"Lỗi phần mềm", "Crash"},
    {"Treo (watchdog)", "Hang (watchdog)"},
    {"Sụt nguồn", "Brownout"},
    {"Khác", "Other"},
    {"WiFi", "WiFi"},
    {"IP", "IP"},
    {"MAC", "MAC"},
    {"RAM trống", "Free RAM"},
    {"%lu KB (thấp nhất %lu KB)", "%lu KB (lowest %lu KB)"},
    {"Thời tiết", "Weather"},
    {"cập nhật %lu phút trước", "updated %lu min ago"},
    {"vừa cập nhật", "just updated"},
    {"chưa có dữ liệu", "no data yet"},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == static_cast<size_t>(Str::Count),
              "kTexts must have one entry per Str id");

const Text kWeekdays[] = {{"CN", "Sun"}, {"T2", "Mon"}, {"T3", "Tue"}, {"T4", "Wed"},
                          {"T5", "Thu"}, {"T6", "Fri"}, {"T7", "Sat"}};
const Text kCompass[] = {{"B", "N"},  {"ĐB", "NE"}, {"Đ", "E"},  {"ĐN", "SE"},
                         {"N", "S"},  {"TN", "SW"}, {"T", "W"},  {"TB", "NW"}};
const Text kUvLevels[] = {
    {"Thấp", "Low"}, {"Trung bình", "Moderate"}, {"Cao", "High"},
    {"Rất cao", "Very high"}, {"Cực cao", "Extreme"}};
const Text kAqiLevels[] = {
    {"Tốt", "Good"}, {"Trung bình", "Moderate"}, {"Kém", "Sensitive"},
    {"Xấu", "Unhealthy"}, {"Rất xấu", "Very bad"}, {"Nguy hại", "Hazardous"}};

struct WeatherText {
  uint8_t code;
  Text text;
};
const WeatherText kWeather[] = {
    {0, {"Trời quang", "Clear sky"}},
    {1, {"Ít mây", "Mainly clear"}},
    {2, {"Mây rải rác", "Partly cloudy"}},
    {3, {"Nhiều mây", "Overcast"}},
    {45, {"Sương mù", "Fog"}},
    {48, {"Sương muối", "Rime fog"}},
    {51, {"Mưa phùn nhẹ", "Light drizzle"}},
    {53, {"Mưa phùn", "Drizzle"}},
    {55, {"Mưa phùn dày", "Dense drizzle"}},
    {56, {"Mưa phùn băng", "Freezing drizzle"}},
    {57, {"Mưa phùn băng", "Freezing drizzle"}},
    {61, {"Mưa nhẹ", "Light rain"}},
    {63, {"Mưa vừa", "Rain"}},
    {65, {"Mưa to", "Heavy rain"}},
    {66, {"Mưa băng", "Freezing rain"}},
    {67, {"Mưa băng", "Freezing rain"}},
    {71, {"Tuyết nhẹ", "Light snow"}},
    {73, {"Tuyết", "Snow"}},
    {75, {"Tuyết dày", "Heavy snow"}},
    {77, {"Hạt tuyết", "Snow grains"}},
    {80, {"Mưa rào nhẹ", "Light showers"}},
    {81, {"Mưa rào", "Showers"}},
    {82, {"Mưa rào to", "Heavy showers"}},
    {85, {"Mưa tuyết", "Snow showers"}},
    {86, {"Mưa tuyết to", "Heavy snow showers"}},
    {95, {"Dông", "Thunderstorm"}},
    {96, {"Dông, mưa đá", "Thunderstorm, hail"}},
    {99, {"Dông, mưa đá", "Thunderstorm, hail"}},
};

constexpr int kDaysPerWeek = 7;
constexpr int kCompassPoints = 8;
constexpr uint16_t kFullCircle = 360;

}  // namespace

void setLanguage(Language lang) { gLanguage = lang; }

Language currentLanguage() { return gLanguage; }

const char* tr(Str id) {
  const size_t i = static_cast<size_t>(id);
  return i < static_cast<size_t>(Str::Count) ? pick(kTexts[i]) : "";
}

const char* weekdayShort(int wday) {
  return wday >= 0 && wday < kDaysPerWeek ? pick(kWeekdays[wday]) : "";
}

const char* weatherText(uint8_t code) {
  for (const auto& w : kWeather) {
    if (w.code == code) {
      return pick(w.text);
    }
  }
  return "--";
}

const char* compassName(uint16_t degrees) {
  // Each point covers 45 degrees centred on it (N = 337.5 .. 22.5).
  const int sector = ((degrees % kFullCircle) * 2 + kFullCircle / kCompassPoints) /
                     (2 * kFullCircle / kCompassPoints) % kCompassPoints;
  return pick(kCompass[sector]);
}

const char* uvLevelName(uint8_t level) {
  constexpr uint8_t kLast = sizeof(kUvLevels) / sizeof(kUvLevels[0]) - 1;
  return pick(kUvLevels[level > kLast ? kLast : level]);
}

const char* aqiLevelName(uint8_t level) {
  constexpr uint8_t kLast = sizeof(kAqiLevels) / sizeof(kAqiLevels[0]) - 1;
  return pick(kAqiLevels[level > kLast ? kLast : level]);
}
