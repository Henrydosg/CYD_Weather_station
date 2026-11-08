"""Render PC previews of the UI screens (black theme).

Mirrors the layout constants of src/ui/screen_*.cpp with the same Roboto fonts
and the icons from gen_icons.py, reads the colours from src/ui/theme.h and
quantises to RGB565 like the panel. Sample data only; panel gamma and viewing
angle are not simulated. Keep it in sync when a screen's layout changes.

Usage (from the project root, needs Pillow):
    python tools/preview_screen.py --out preview.png [--scale 2] [--screen main|setup|wifi|info]
      main:  dashboard (Tet countdown, rain soon, raining, full moon) and the 24 h graph
      setup: settings, city manager, action panel, search keyboard, search results,
             photo viewer hint and message, firmware update (available, downloading),
             remote message
      wifi:  WiFi list, password keyboard, connect error
"""

import argparse
import math
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_icons  # noqa: E402

FONT_DIR = Path("C:/Windows/Fonts")
W, H = 320, 240
SUP = 4  # supersampling for anti-aliased shapes
CARD_RADIUS = 10


def font(name, size):
    return ImageFont.truetype(str(FONT_DIR / name), size)


F = {
    "tiny": font("Roboto-Regular.ttf", 12), "tinyBold": font("Roboto-Medium.ttf", 12),
    "label": font("Roboto-Regular.ttf", 13), "small": font("Roboto-Regular.ttf", 15),
    "medium": font("Roboto-Medium.ttf", 20), "big": font("Roboto-Medium.ttf", 40),
}


def q565(c):
    """What the panel shows for an 8-bit colour: RGB565, expanded back to 8 bits."""
    r, g, b = c[0] >> 3, c[1] >> 2, c[2] >> 3
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def load_colours():
    hdr = (ROOT / "src/ui/theme.h").read_text(encoding="utf-8")
    src = (ROOT / "src/ui/theme.cpp").read_text(encoding="utf-8")
    pat = r"constexpr uint16_t k(\w+) = rgb\((\d+), (\d+), (\d+)\);"
    return {n: q565((int(r), int(g), int(b))) for n, r, g, b in re.findall(pat, hdr + src)}


C = load_colours()
ICON_FN = dict(gen_icons.WEATHER_ICONS)
_icons = {}


def icon(name, small=False):
    key = (name, small)
    if key not in _icons:
        _icons[key] = gen_icons.render(ICON_FN[name], gen_icons.BASE, 30 if small else None)
    return _icons[key]


def new_screen():
    img = Image.new("RGB", (W, H), C["Background"])
    return img, ImageDraw.Draw(img)


def rrect(img, box, r, fill):
    x0, y0, x1, y1 = [int(v) for v in box]
    w, h = x1 - x0 + 1, y1 - y0 + 1
    m = Image.new("L", (w * SUP, h * SUP), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, w * SUP - 1, h * SUP - 1), r * SUP, fill=255)
    img.paste(Image.new("RGB", (w, h), fill), (x0, y0), m.resize((w, h), Image.LANCZOS))


def card(img, x, y, w, h, r=CARD_RADIUS):
    """ui::drawCard: border-coloured rounded rect, background inset by 1 px."""
    rrect(img, (x, y, x + w - 1, y + h - 1), r, C["CardBorder"])
    rrect(img, (x + 1, y + 1, x + w - 2, y + h - 2), r - 1, C["Background"])


def pill(img, x, y, w, h, label, fill, fg, outline, fnt="small"):
    rrect(img, (x, y, x + w - 1, y + h - 1), h // 2, fill)
    if outline:
        rrect(img, (x + 1, y + 1, x + w - 2, y + h - 2), h // 2 - 1, C["Background"])
    ImageDraw.Draw(img).text((x + w / 2, y + h / 2), label, font=F[fnt], fill=fg, anchor="mm")


def chevron(d, cx, cy, right, color):
    s = 1 if right else -1
    d.line((cx - s * 2, cy - 5, cx + s * 2, cy, cx - s * 2, cy + 5), fill=color, width=2)


def bars(d, x, bottom, lit, on):
    for i in range(4):
        h = 4 + i * 3
        d.rectangle((x + i * 5, bottom - h, x + i * 5 + 2, bottom - 1),
                    fill=on if i < lit else C["BarOff"])


def back_header(img, title):
    d = ImageDraw.Draw(img)
    chevron(d, 14, 18, False, C["Ink"])
    d.text((26, 18), title, font=F["medium"], fill=C["Ink"], anchor="lm")


def paste(img, im, x, y):
    img.paste(im, (int(x), int(y)), im)


def temp_color(t):
    return C["HotColor"] if t >= 36 else C["WarmColor"] if t >= 33 else (
        C["Low"] if t <= 22 else C["Temp"])


def level_color(level):
    return [C["Ok"], C["Warn"], C["Alert"], C["Error"], C["Severe"], C["Danger"]][min(level, 5)]


# ---------------------------------------------------------------------------
# Sample data
# ---------------------------------------------------------------------------

CITIES = [("TP. Hồ Chí Minh", "Việt Nam"), ("Hà Nội", "Việt Nam"), ("Hải Phòng", "Việt Nam"),
          ("Đà Nẵng", "Việt Nam"), ("Đà Lạt", "Lâm Đồng, Việt Nam"),
          ("Calgary", "Alberta, Canada"), ("Melbourne", "Victoria, Úc")]
WEATHER = {
    0: dict(time="14:35", date="T7, 01/11", lunar="ÂL 23/9", desc="Mây rải rác", icon="kIconPartlyDay", temp=31,
            feels=37, rows=["05:42", "17:39", "74%", "11 km/h ĐN", "1009 hPa", "60%"],
            uv=(8, "Rất cao", 3), aqi=(54, "Trung bình", 1),
            days=[("T7", "31/10", "kIconPartlyDay", 33, 25), ("CN", "1/11", "kIconRain", 31, 25),
                  ("T2", "2/11", "kIconThunder", 30, 24), ("T3", "3/11", "kIconPartlyDay", 32, 25),
                  ("T4", "4/11", "kIconClearDay", 33, 24), ("T5", "5/11", "kIconDrizzle", 31, 25),
                  ("T6", "6/11", "kIconCloudy", 32, 25)]),
    5: dict(time="01:35", date="T7, 31/10", lunar="ÂL 22/9", desc="Tuyết nhẹ", icon="kIconSnow", temp=-3,
            feels=-9, rows=["08:12", "18:04", "81%", "24 km/h TB", "1021 hPa", "90%"],
            uv=(0, "Thấp", 0), aqi=(18, "Tốt", 0),
            days=[("T7", "31/10", "kIconSnow", 2, -6), ("CN", "1/11", "kIconCloudy", 4, -5),
                  ("T2", "2/11", "kIconPartlyDay", 7, -3), ("T3", "3/11", "kIconClearDay", 9, -2),
                  ("T4", "4/11", "kIconPartlyDay", 6, -4), ("T5", "5/11", "kIconSnow", 1, -7),
                  ("T6", "6/11", "kIconCloudy", 3, -6)]),
}
HOURLY = [(15, 32, "kIconPartlyDay", 20), (18, 30, "kIconRain", 60), (21, 28, "kIconPartlyNight", 30),
          (0, 27, "kIconClearNight", 10), (3, 26, "kIconClearNight", 5), (6, 26, "kIconPartlyDay", 10),
          (9, 29, "kIconClearDay", 15), (12, 31, "kIconPartlyDay", 40)]
LABELS = ["Mặt trời mọc", "Mặt trời lặn", "Độ ẩm", "Gió", "Áp suất", "Mây", "Chỉ số UV", "AQI"]


# ---------------------------------------------------------------------------
# Screens
# ---------------------------------------------------------------------------

def drop(img, cx, cy, color):
    """Rain drop before rain notes: triangle tip + circle (screen_dashboard.cpp drawNote)."""
    s = SUP
    layer = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.polygon([((cx - 3) * s, cy * s), ((cx + 3) * s, cy * s), (cx * s, (cy - 6) * s)],
               fill=color + (255,))
    ld.ellipse(((cx - 4) * s, (cy - 3) * s, (cx + 4) * s, (cy + 5) * s), fill=color + (255,))
    small = layer.resize((W, H), Image.LANCZOS)
    img.paste(small, (0, 0), small)


def dashboard(city, note=("Còn 97 ngày đến Tết", False), lunar=None, date=None):
    """screen_dashboard.cpp; note = (text, rain)"""
    img, d = new_screen()
    name, region = CITIES[city]
    w = WEATHER[city]
    d.text((8, 12), name, font=F["medium"], fill=C["Ink"], anchor="lm")
    d.text((8, 31), region, font=F["tiny"], fill=C["Muted"], anchor="lm")
    d.text((288, 12), w["time"], font=F["medium"], fill=C["Highlight"], anchor="rm")
    lunar, date = lunar or w["lunar"], date or w["date"]
    d.text((312, 31), lunar, font=F["tiny"], fill=C["Label"], anchor="rm")
    d.text((312 - d.textlength(lunar, font=F["tiny"]) - 8, 31), date, font=F["tiny"],
           fill=C["Highlight"], anchor="rm")
    bars(d, 294, 18, 3, C["Ok"])
    d.line((6, 40, 313, 40), fill=C["Line"])
    n = len(CITIES)
    x0 = 160 - (n - 1) * 10 // 2
    d.rectangle((x0 - 6, 37, x0 + (n - 1) * 10 + 6, 43), fill=C["Background"])
    for i in range(n):
        r = 3 if i == city else 2
        d.ellipse((x0 + i * 10 - r, 40 - r, x0 + i * 10 + r, 40 + r),
                  fill=C["Highlight"] if i == city else C["CardBorder"])

    d.text((76, 54), w["desc"], font=F["medium"], fill=C["Description"], anchor="mm")
    paste(img, icon(w["icon"]), 2, 66)
    d.text((112, 98), f"{w['temp']}°", font=F["big"], fill=temp_color(w["temp"]), anchor="mm")
    d.text((112, 130), f"Cảm giác {w['feels']}°", font=F["tiny"], fill=C["Feels"], anchor="mm")
    text, rain = note
    color = C["Description"] if rain else C["Feels"]
    x = 76 - (d.textlength(text, font=F["tiny"]) + (12 if rain else 0)) // 2
    if rain:
        drop(img, int(x) + 4, 147, color)
        d = ImageDraw.Draw(img)
        x += 12
    d.text((x, 147), text, font=F["tiny"], fill=color, anchor="lm")

    values = w["rows"] + [f"{w['uv'][0]} {w['uv'][1]}", f"{w['aqi'][0]} {w['aqi'][1]}"]
    colors = [C["Ink"]] * 6 + [level_color(w["uv"][2]), level_color(w["aqi"][2])]
    for i, (lab, val, col) in enumerate(zip(LABELS, values, colors)):
        y = 50 + i * 14
        d.text((230, y), lab + ":", font=F["label"], fill=C["Label"], anchor="rm")
        d.text((234, y), val, font=F["label"], fill=col, anchor="lm")

    d.line((6, 158, 313, 158), fill=C["Line"])
    colw = 308 // 7
    for i, (dn, dt, ic, hi, lo) in enumerate(w["days"]):
        x = 6 + i * colw
        cx = x + colw // 2
        if i == 0:
            rrect(img, (x + 1, 161, x + colw - 2, 237), 6, C["Track"])
            d = ImageDraw.Draw(img)
        d.text((cx, 169), dn, font=F["tinyBold"], fill=C["Ink"], anchor="mm")
        d.text((cx, 182), dt, font=F["tiny"], fill=C["Muted"], anchor="mm")
        paste(img, icon(ic, True), cx - 15, 189)
        d.text((cx, 222), f"{hi}°", font=F["tinyBold"], fill=C["High"], anchor="mm")
        d.text((cx, 234), f"{lo}°", font=F["tinyBold"], fill=C["Low"], anchor="mm")
    return img


def graph():
    """screen_graph.cpp"""
    img, d = new_screen()
    d.text((8, 12), CITIES[0][0], font=F["medium"], fill=C["Ink"], anchor="lm")
    d.text((8, 31), "Nhiệt độ 24 giờ tới", font=F["tiny"], fill=C["Muted"], anchor="lm")
    d.text((312, 12), "14:35", font=F["medium"], fill=C["Highlight"], anchor="rm")
    d.line((6, 40, 313, 40), fill=C["Line"])
    temps = [p[1] for p in HOURLY]
    lo, span = min(temps), max(max(temps) - min(temps), 1)
    xs = [22 + i * 39 for i in range(len(HOURLY))]
    ys = [176 - round((t - lo) * (176 - 108) / span) for t in temps]
    layer = Image.new("RGBA", (W * SUP, H * SUP), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.line([(x * SUP, y * SUP) for x, y in zip(xs, ys)], fill=C["Graph"] + (255,), width=2 * SUP)
    for x, y in zip(xs, ys):
        ld.ellipse(((x - 3) * SUP, (y - 3) * SUP, (x + 3) * SUP, (y + 3) * SUP), fill=C["Graph"] + (255,))
    img.paste(layer.resize((W, H), Image.LANCZOS), (0, 0), layer.resize((W, H), Image.LANCZOS))
    d = ImageDraw.Draw(img)
    for (hour, t, ic, rain), x, y in zip(HOURLY, xs, ys):
        paste(img, icon(ic, True), x - 15, y - 36)
        d = ImageDraw.Draw(img)
        d.text((x, y - 44), f"{t}°", font=F["tinyBold"], fill=C["Temp"], anchor="mm")
        d.text((x, y - 57), f"{hour}h", font=F["tiny"], fill=C["Muted"], anchor="mm")
        bh = rain * 30 // 100
        if bh:
            d.rectangle((x - 6, 226 - bh, x + 5, 225), fill=C["RainBar"])
        d.text((x, 234), f"{rain}%", font=F["tiny"], fill=C["Low"], anchor="mm")
    d.line((6, 194, 313, 194), fill=C["Line"])
    d.text((8, 202), "Khả năng mưa", font=F["tiny"], fill=C["Muted"], anchor="lm")
    return img


def settings():
    """screen_settings.cpp"""
    img, _ = new_screen()
    back_header(img, "Cài đặt")
    pill(img, 228, 4, 84, 28, "Ảnh", C["Accent"], C["Accent"], True)
    pill(img, 160, 4, 60, 28, "", C["CardBorder"], C["Ink"], True)
    d = ImageDraw.Draw(img)
    d.text((176, 18), "VI", font=F["small"], fill=C["Highlight"], anchor="mm")
    d.text((204, 18), "EN", font=F["small"], fill=C["Muted"], anchor="mm")
    d.line((190, 12, 190, 23), fill=C["CardBorder"])
    rows = [("Thành phố", "7 / 8", C["Highlight"]), ("Đổi thành phố sau", "20 giây", C["Highlight"]),
            ("Độ sáng", "Tự động 100%", C["Highlight"]), ("Hiệu chuẩn cảm ứng", "", C["Muted"]),
            ("WiFi", "NhaRieng", C["Ok"]), ("Firmware", "Có bản mới v1.1.0", C["Highlight"])]
    for i, (lab, val, col) in enumerate(rows):
        top = 40 + i * 33
        mid = top + 15
        card(img, 6, top, 308, 30)
        d = ImageDraw.Draw(img)
        d.text((18, mid), lab, font=F["small"], fill=C["Ink"], anchor="lm")
        d.text((288, mid), val, font=F["small"], fill=col, anchor="rm")
        chevron(d, 302, mid, True, C["Muted"])
    return img


def update_screen(downloading=False):
    """screen_update.cpp: new version available / downloading"""
    img, d = new_screen()
    back_header(img, "Cập nhật firmware")
    d.text((160, 70), "Phiên bản v1.0.0", font=F["medium"], fill=C["Ink"], anchor="mm")
    d.text((160, 102), "Có bản mới v1.1.0", font=F["small"], fill=C["Highlight"], anchor="mm")
    if downloading:
        bar_w, filled = 260, 260 * 42 // 100
        d.rectangle((30, 122, 30 + filled - 1, 133), fill=C["Accent"])
        d.rectangle((30 + filled, 122, 30 + bar_w - 1, 133), fill=C["Track"])
        d.text((160, 170), "Đang tải 42%", font=F["tiny"], fill=C["Ink"], anchor="mm")
    d.text((160, 152), "Không rút điện khi đang cập nhật", font=F["tiny"], fill=C["Muted"],
           anchor="mm")
    if not downloading:
        pill(img, 70, 182, 180, 40, "Cập nhật", C["Accent"], C["OnAccent"], False)
    return img


def update_screen_info(rolled_back=False):
    """screen_update.cpp with the "Thông tin" header button (and a rollback notice)"""
    img, d = new_screen()
    back_header(img, "Firmware")
    pill(img, 212, 4, 100, 28, "Thông tin", C["Accent"], C["Accent"], True)
    d.text((160, 70), "Phiên bản v1.1.5", font=F["medium"], fill=C["Ink"], anchor="mm")
    if rolled_back:
        d.text((160, 102), "Bản v1.2.1 bị lỗi, đã quay lại", font=F["small"], fill=C["Warn"],
               anchor="mm")
        d.text((160, 152), "Máy tự quay lại bản trước để vẫn chạy được", font=F["tiny"],
               fill=C["Muted"], anchor="mm")
        pill(img, 70, 182, 180, 40, "Kiểm tra", C["Accent"], C["OnAccent"], False)
    else:
        d.text((160, 102), "Đã là bản mới nhất", font=F["small"], fill=C["Ok"], anchor="mm")
        pill(img, 70, 182, 180, 40, "Kiểm tra", C["Accent"], C["OnAccent"], False)
    return img


def info_screen(verifying=False):
    """screen_info.cpp: device information (opened from the Firmware screen)"""
    img, d = new_screen()
    back_header(img, "Thông tin máy")
    version = ("v1.1.5  đang kiểm tra (8 phút)", C["Highlight"]) if verifying else (
        "v1.1.5  đã xác nhận", C["Ok"])
    rows = [("Phiên bản", *version), ("Đang chạy", "2 ngày 05:14", C["Ink"]),
            ("Khởi động", "37 lần, lỗi 1 lần", C["Ink"]),
            ("Lần cuối do", "Rút điện / bật lại", C["Ink"]),
            ("WiFi", "MIX   -58 dBm", C["Ok"]), ("IP", "172.19.97.143", C["Ink"]),
            ("MAC", "24:6F:28:AB:CD:EF", C["Ink"]), ("RAM trống", "142 KB (thấp nhất 96 KB)", C["Ink"]),
            ("Thời tiết", "cập nhật 3 phút trước", C["Ink"])]
    card(img, 6, 38, 308, 196)
    d = ImageDraw.Draw(img)
    for i, (lab, val, col) in enumerate(rows):
        mid = 50 + i * 21
        d.text((16, mid), lab, font=F["label"], fill=C["Label"], anchor="lm")
        d.text((304, mid), val, font=F["label"], fill=col, anchor="rm")
        if i < len(rows) - 1:
            d.line((16, mid + 10, 304, mid + 10), fill=C["Track"])
    return img


def wrap(d, text, fnt, max_w):
    lines, cur = [], ""
    for word in text.split():
        t = (cur + " " + word).strip()
        if d.textlength(t, font=fnt) <= max_w or not cur:
            cur = t
        else:
            lines.append(cur)
            cur = word
    return lines + [cur]


def message_screen():
    """screen_message.cpp"""
    img, d = new_screen()
    d.rectangle((12, 9, 37, 26), outline=C["Highlight"], width=2)
    d.line((13, 10, 25, 19), fill=C["Highlight"], width=2)
    d.line((36, 10, 25, 19), fill=C["Highlight"], width=2)
    d.text((48, 18), "Lời nhắn từ Henry", font=F["medium"], fill=C["Highlight"], anchor="lm")
    d.line((6, 40, 313, 40), fill=C["Line"])
    text = ("Chúc mừng sinh nhật bạn! Chúc bạn một tuổi mới thật nhiều sức khỏe, "
            "niềm vui và luôn có ngày nắng đẹp. Hẹn gặp lại cuối tuần nhé!")
    for i, line in enumerate(wrap(d, text, F["small"], 288)[:6]):
        d.text((16, 58 + i * 21), line, font=F["small"], fill=C["Ink"], anchor="lm")
    pill(img, 90, 190, 140, 40, "Đã đọc", C["Accent"], C["OnAccent"], False)
    return img


def photos_hint():
    """screen_photos.cpp: tap zones shown when the viewer opens"""
    img, d = new_screen()
    d.text((160, 20), "Ảnh", font=F["medium"], fill=C["Ink"], anchor="mm")
    zone = 320 // 3
    for i in (1, 2):
        d.line((i * zone, 44, i * zone, 195), fill=C["Line"])
    mid = (44 + 196) // 2
    for cx, label, right in ((zone // 2, "Trước", False), (320 - zone // 2, "Sau", True)):
        d.text((cx, mid), label, font=F["small"], fill=C["Accent"], anchor="mm")
        half = d.textlength(label, font=F["small"]) / 2
        chevron(d, cx + (half + 10 if right else -half - 10), mid, right, C["Accent"])
    d.text((160, mid), "Về thời tiết", font=F["small"], fill=C["Muted"], anchor="mm")
    d.text((160, 218), "12 ảnh, tự chuyển sau 15 giây", font=F["tiny"], fill=C["Muted"],
           anchor="mm")
    return img


def photos_message():
    """screen_photos.cpp: no card / no photos"""
    img, d = new_screen()
    d.text((160, 108), "Không có ảnh .jpg trong /photos", font=F["small"], fill=C["Error"],
           anchor="mm")
    d.text((160, 136), "Về thời tiết", font=F["tiny"], fill=C["Muted"], anchor="mm")
    return img


def cities(panel=False):
    """screen_cities.cpp"""
    img, _ = new_screen()
    back_header(img, f"Thành phố  {len(CITIES)}/8")
    pill(img, 228, 4, 84, 28, "+ Thêm", C["Accent"], C["Accent"], True)
    d = ImageDraw.Draw(img)
    for i, (name, region) in enumerate(CITIES):
        top = 40 + i * 25
        mid = top + 12
        if panel and i == 5:
            rrect(img, (4, top + 1, 315, top + 23), 6, C["Track"])
            d = ImageDraw.Draw(img)
        if i > 0:
            d.line((12, top, 307, top), fill=C["Line"])
        d.text((16, mid), str(i + 1), font=F["tinyBold"], fill=C["Muted"], anchor="mm")
        d.text((32, mid), name, font=F["small"], fill=C["Ink"], anchor="lm")
        d.text((290, mid), region.split(", ")[-1], font=F["tiny"], fill=C["Muted"], anchor="rm")
        chevron(d, 304, mid, True, C["Muted"])
    if panel:
        card(img, 30, 60, 260, 130)
        d = ImageDraw.Draw(img)
        d.text((160, 82), "Calgary", font=F["medium"], fill=C["Ink"], anchor="mm")
        d.text((160, 102), "Alberta, Canada", font=F["tiny"], fill=C["Muted"], anchor="mm")
        for i, (lab, fill) in enumerate((("Lên", C["Track"]), ("Xuống", C["Track"]), ("Xoá", C["Danger"]))):
            pill(img, 44 + i * 80, 120, 72, 32, lab, fill, C["OnAccent"], False)
        ImageDraw.Draw(img).text((160, 172), "Đóng", font=F["small"], fill=C["Accent"], anchor="mm")
    return img


def keyboard(title, text, masked, submit, layout="lower"):
    """screen_keyboard.cpp"""
    img, _ = new_screen()
    back_header(img, title)
    rrect(img, (6, 34, 313, 67), 10, C["Accent"])
    rrect(img, (7, 35, 312, 66), 9, C["Background"])
    d = ImageDraw.Draw(img)
    shown = "*" * len(text) if masked else text
    d.text((16, 51), shown, font=F["medium"], fill=C["Ink"], anchor="lm")
    tw = d.textlength(shown, font=F["medium"])
    d.rectangle((17 + tw, 41, 18 + tw, 60), fill=C["Accent"])
    if masked:
        d.text((304, 51), "Hiện", font=F["small"], fill=C["Accent"], anchor="rm")

    kw, kh, gap, y0 = 29, 36, 3, 74
    full = 10 * kw + 9 * gap
    x0 = (W - full) // 2
    wide = (full - 7 * (kw + gap) - gap) // 2
    rows = (["1234567890", "@#$%&*-+()", "_!?/:;'\"."] if layout == "symbols"
            else ["qwertyuiop", "asdfghjkl", "zxcvbnm"])
    if layout == "upper":
        rows = [r.upper() for r in rows]

    def key(x, y, w, label, accent=False, fnt="medium"):
        rrect(img, (x, y, x + w - 1, y + kh - 1), 6, C["Accent"] if accent else C["Track"])
        ImageDraw.Draw(img).text((x + w / 2, y + kh / 2), label, font=F[fnt],
                                 fill=C["OnAccent"] if accent else C["Ink"], anchor="mm")

    for r in range(2):
        first = x0 + (10 - len(rows[r])) * (kw + gap) // 2
        for i, ch in enumerate(rows[r]):
            key(first + i * (kw + gap), y0 + r * (kh + gap), kw, ch)
    y3 = y0 + 2 * (kh + gap)
    cy = y3 + kh / 2
    if layout == "symbols":
        first = x0
    else:
        first = x0 + wide + gap
        key(x0, y3, wide, "", accent=layout == "upper")
        cx = x0 + wide / 2
        col = C["OnAccent"] if layout == "upper" else C["Ink"]
        ImageDraw.Draw(img).line((cx - 4, cy + 7, cx - 4, cy, cx - 8, cy, cx, cy - 8, cx + 8, cy,
                                  cx + 4, cy, cx + 4, cy + 7, cx - 4, cy + 7), fill=col, width=2)
    for i, ch in enumerate(rows[2]):
        key(first + i * (kw + gap), y3, kw, ch)
    bx = first + len(rows[2]) * (kw + gap)
    key(bx, y3, x0 + full - bx, "")
    cx = bx + (x0 + full - bx) / 2
    dd = ImageDraw.Draw(img)
    dd.line((cx - 10, cy, cx - 4, cy - 7, cx + 10, cy - 7, cx + 10, cy + 7, cx - 4, cy + 7, cx - 10, cy),
            fill=C["Ink"], width=2)
    dd.line((cx - 1, cy - 3, cx + 5, cy + 3), fill=C["Ink"], width=2)
    dd.line((cx - 1, cy + 3, cx + 5, cy - 3), fill=C["Ink"], width=2)
    y4 = y3 + kh + gap
    key(x0, y4, wide, "abc" if layout == "symbols" else "123", fnt="small")
    sx = x0 + wide + gap
    key(sx, y4, 118, "dấu cách", fnt="small")
    key(sx + 121, y4, 48, "Huỷ", fnt="small")
    key(sx + 172, y4, x0 + full - sx - 172, submit, accent=True, fnt="small")
    return img


def search_results():
    """screen_search.cpp"""
    img, _ = new_screen()
    back_header(img, "Kết quả: melbourne")
    res = [("Melbourne", "Victoria, Úc", "5,0 tr. dân", True), ("Melbourne", "Florida, Mỹ", "84 nghìn dân", True),
           ("Melbourne", "Iowa, Mỹ", "830 dân", True), ("Melbourne", "Kentucky, Mỹ", "440 dân", True),
           ("Melbourne", "Arkansas, Mỹ", "1,8 nghìn dân", True)]
    for i, (name, region, pop, add) in enumerate(res):
        top = 42 + i * 39
        mid = top + 17
        card(img, 6, top, 308, 35)
        d = ImageDraw.Draw(img)
        d.text((16, mid - 8), name, font=F["small"], fill=C["Ink"], anchor="lm")
        d.text((16, mid + 9), region, font=F["tiny"], fill=C["Muted"], anchor="lm")
        d.text((262, mid - 8), pop, font=F["tiny"], fill=C["Muted"], anchor="rm")
        pill(img, 272, mid - 12, 32, 24, "+", C["Accent"] if add else C["Track"],
             C["OnAccent"] if add else C["Disabled"], False, "medium")
    return img


def wifi_list():
    """screen_wifi.cpp"""
    img, _ = new_screen()
    back_header(img, "WiFi")
    pill(img, 220, 4, 92, 28, "Quét lại", C["Accent"], C["Accent"], True)
    d = ImageDraw.Draw(img)
    d.text((10, 46), "Đã kết nối: NhaRieng - 192.168.1.23", font=F["small"], fill=C["Ok"], anchor="lm")
    card(img, 6, 60, 308, 144)
    d = ImageDraw.Draw(img)
    nets = [("NhaRieng", 4, True, False, True), ("Cafe_Sua_Da", 3, False, True, False),
            ("VNPT-5G-A1B2", 2, True, False, False), ("Viettel_Home", 1, True, False, False)]
    for i, (ssid, lit, secured, saved, connected) in enumerate(nets):
        top, mid = 60 + i * 36, 60 + i * 36 + 18
        if i > 0:
            d.line((16, top, 303, top), fill=C["Line"])
        for b in range(4):
            h = 4 + b * 3
            d.rectangle((18 + b * 5, mid + 7 - h, 20 + b * 5, mid + 6),
                        fill=C["Accent"] if b < lit else C["BarOff"])
        d.text((46, mid), ssid, font=F["medium"], fill=C["Ink"], anchor="lm")
        if saved and not connected:
            d.text((302, mid), "Đã lưu", font=F["small"], fill=C["Muted"], anchor="rm")
            continue
        if secured:
            d.rounded_rectangle((262, mid - 2, 272, mid + 5), 2, fill=C["Muted"])
            d.arc((263, mid - 7, 271, mid + 1), 180, 360, fill=C["Muted"], width=2)
        if connected:
            d.line((285, mid, 289, mid + 4, 296, mid - 4), fill=C["Ok"], width=2)
        else:
            chevron(d, 294, mid, True, C["Muted"])
    for x0, label, enabled in ((6, "Trước", False), (214, "Sau", True)):
        pill(img, x0, 210, 100, 28, label, C["Track"], C["Ink"] if enabled else C["Disabled"], False)
    ImageDraw.Draw(img).text((160, 224), "1 / 2", font=F["small"], fill=C["Muted"], anchor="mm")
    return img


def connect_failed():
    """screen_connect.cpp"""
    img, _ = new_screen()
    back_header(img, "VNPT-5G-A1B2")
    card(img, 30, 70, 260, 110)
    d = ImageDraw.Draw(img)
    d.text((160, 98), "Sai mật khẩu hoặc", font=F["small"], fill=C["Error"], anchor="mm")
    d.text((160, 120), "không kết nối được", font=F["small"], fill=C["Error"], anchor="mm")
    pill(img, 50, 140, 104, 28, "Nhập lại", C["Accent"], C["OnAccent"], False)
    pill(img, 166, 140, 104, 28, "Quên mạng", C["Accent"], C["Accent"], True)
    return img


SCREENS = {
    "main": lambda: [dashboard(0), dashboard(0, ("Mưa sau ~25 phút", True)),
                     dashboard(0, ("Đang mưa, tạnh ~16:30", True)),
                     dashboard(0, ("Hôm nay rằm tháng 10", False), "ÂL 15/10", "T3, 24/11"),
                     dashboard(5, ("Mưa lúc ~16:30", True)), graph()],
    "setup": lambda: [settings(), cities(), cities(panel=True),
                      keyboard("Thêm thành phố", "melbourne", False, "Tìm"), search_results(),
                      photos_hint(), photos_message(), update_screen(),
                      update_screen(downloading=True), message_screen()],
    "wifi": lambda: [wifi_list(), keyboard("VNPT-5G-A1B2", "matkhau12", True, "Kết nối"),
                     keyboard("VNPT-5G-A1B2", "matkhau12", True, "Kết nối", "symbols"),
                     connect_failed()],
    "info": lambda: [update_screen_info(), update_screen_info(rolled_back=True), info_screen(),
                     info_screen(verifying=True)],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, help="PNG sheet to write")
    parser.add_argument("--scale", type=int, default=2, help="pixel scale (nearest neighbour)")
    parser.add_argument("--screen", choices=sorted(SCREENS), default="main")
    args = parser.parse_args()

    images = SCREENS[args.screen]()
    cols, sc, pad = 3, args.scale, 12
    rows = math.ceil(len(images) / cols)
    sheet = Image.new("RGB", (cols * (W * sc + pad) + pad, rows * (H * sc + pad) + pad), (40, 40, 40))
    for i, img in enumerate(images):
        px = img.load()
        for y in range(H):
            for x in range(W):
                px[x, y] = q565(px[x, y])
        x, y = pad + (i % cols) * (W * sc + pad), pad + (i // cols) * (H * sc + pad)
        sheet.paste(img.resize((W * sc, H * sc), Image.NEAREST), (x, y))
    sheet.save(args.out)
    print(f"preview: {args.out}")


if __name__ == "__main__":
    main()
