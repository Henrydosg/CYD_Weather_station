# CYD Weather Station

Trạm thời tiết trên board **ESP32-2432S028R** ("Cheap Yellow Display", màn 2.8" ILI9341,
cảm ứng điện trở). Dữ liệu từ [Open-Meteo](https://open-meteo.com/), miễn phí, không cần
API key.

- Tối đa 8 thành phố, tìm và thêm ngay trên máy; màn chính tự đổi thành phố sau 10/20/30/60 giây.
- Thời tiết hiện tại, chi tiết (mặt trời mọc/lặn, độ ẩm, gió, áp suất, mây, UV, AQI),
  dự báo 7 ngày và đồ thị nhiệt độ + khả năng mưa 24 giờ.
- Tiếng Việt (tên thành phố có dấu) hoặc tiếng Anh (tên quốc tế).
- Chọn WiFi trên máy, nhớ tối đa 5 mạng.
- Xem ảnh JPEG từ thẻ nhớ, tự chuyển ảnh như khung ảnh.

## Build và nạp

Cần VS Code + PlatformIO.

1. Chép `include/secrets.example.h` thành `include/secrets.h`, điền WiFi mặc định
   (file này không được commit).
2. Build và nạp bằng PlatformIO trong VS Code (môi trường `cyd`).
3. Lần chạy đầu máy tự mở màn **hiệu chuẩn cảm ứng**: chạm lần lượt vào hai dấu ngắm.

Cài đặt (thành phố, WiFi, ngôn ngữ, hiệu chuẩn...) lưu trong bộ nhớ NVS, giữ nguyên khi
nạp firmware mới; chỉ mất khi xoá flash.

## Sử dụng

| Màn hình | Chạm |
|---|---|
| Màn chính | Dải trên cùng (tên thành phố, giờ) → **Cài đặt**; phần còn lại → **đồ thị 24 giờ**. Mỗi lần chạm, thành phố dừng đổi 60 giây. |
| Đồ thị | Chạm bất kỳ → về màn chính. |
| Cài đặt | Thành phố, thời gian đổi thành phố, ngôn ngữ, hiệu chuẩn cảm ứng, WiFi; nút **Ảnh** ở góc phải trên. |
| Thành phố | **+ Thêm** → gõ tên không dấu (ví dụ `da lat`) → **Tìm** → **+** để thêm. Chạm một thành phố để Lên / Xuống / Xoá. |
| WiFi | Chạm mạng để kết nối (mạng có khoá sẽ hỏi mật khẩu). |

Dấu `:` ở đồng hồ chớp mỗi giây cho biết máy đang chạy. Để yên ở Cài đặt, danh sách,
bàn phím hay đồ thị 2 phút thì máy tự về màn chính.

## Xem ảnh từ thẻ nhớ

1. Thẻ microSD định dạng **FAT32**, tạo thư mục **`/photos`**.
2. Chép ảnh **JPEG** vào đó, tốt nhất **320×240**, loại thường (baseline, không phải
   progressive). Ảnh lớn hơn được thu nhỏ 1/2, 1/4, 1/8; tối đa 64 ảnh, xếp theo tên.
3. Cài đặt → **Ảnh**. Màn gợi ý hiện 2 giây rồi tới ảnh đầu tiên; ảnh tự chuyển sau 15 giây.

| Chạm | Tác dụng |
|---|---|
| 1/3 trái / 1/3 phải | Ảnh trước / ảnh sau |
| 1/3 giữa | **Về màn thời tiết** |

Màu ảnh đã được chỉnh sẵn cho panel này: gamma **nạp lại** (`kPanelGamma = Reselect`) và bù
tông màu **mức 1/4** (`kPhotoTone = 1`) trong `include/config.h`. Muốn đổi thì sửa hai giá
trị đó rồi nạp lại.

Có thể rút / cắm thẻ: mỗi lần mở màn Ảnh máy đọc lại thẻ.

Ảnh mẫu để kiểm tra màu: `python tools/make_test_chart.py --out previews/00_test_chart.jpg`,
chép vào `/photos`; thang xám phải đều từ đen tới trắng.

## Khi gặp sự cố

- **Màu bị âm bản toàn màn hình:** đổi `kInvertDisplay` trong `include/config.h`
  (panel TPM408-2.8 cần `true`).
- **Ảnh bệch, ngả xanh:** nhìn thẳng màn hình (panel TN rất kém khi nhìn chếch từ trên
  xuống); nếu vẫn bệch, tăng `kPhotoTone` (0 – 4) trong `include/config.h`.
- **Chạm lệch:** Cài đặt → Hiệu chuẩn cảm ứng.
- **"Không thấy thẻ nhớ":** kiểm tra thẻ FAT32, cắm chặt, rồi mở lại màn Ảnh.
- **Không có dữ liệu thời tiết:** kiểm tra WiFi trong Cài đặt; log ở Serial Monitor 115200 baud.

## Ghi chú kỹ thuật

Chi tiết kiến trúc, GPIO, quy tắc code: xem [CLAUDE.md](CLAUDE.md). Xem trước giao diện trên
PC: `python tools/preview_screen.py --out previews/main.png --screen main|setup|wifi`.
