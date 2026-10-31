"""Render PC previews of the main screen for several weather states.

Mirrors src/ui/screen_main.cpp, analog_clock.cpp and draw_utils.cpp (same
layout, Roboto fonts and icons from gen_icons.py), reads the colours from
src/ui/theme.h, and quantises to RGB565 like the panel. Panel gamma and
viewing angle are not simulated.

Usage (from the project root, needs Pillow):
    python tools/preview_screen.py --out preview.png [--scale 2] [--lang en]
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
# ui::drawShadow (draw_utils.h) and the clock face shadow (analog_clock.cpp)
SHADOW_SPREAD, SHADOW_OFFSET_Y, SHADOW_ALPHA = 6, 3, 36
FACE_SHADOW_SPREAD, FACE_SHADOW_OFFSET_Y = 4, 2

TEXT = {
    "vi": dict(city="TP. Hồ Chí Minh", feels="Cảm giác", hum="Độ ẩm", wind="Gió",
               pres="Áp suất", age="5 phút trước", tap="Chạm để làm mới",
               date="Thứ Bảy, 31/10", pill="VI"),
    "en": dict(city="Ho Chi Minh City", feels="Feels like", hum="Humidity", wind="Wind",
               pres="Pressure", age="5 min ago", tap="Tap to refresh",
               date="Saturday, 31/10", pill="EN"),
}
# (icon, temperature, description vi, description en)
STATES = [
    ("kIconClearDay", 33, "Trời quang", "Clear sky"),
    ("kIconPartlyDay", 31, "Mây rải rác", "Scattered clouds"),
    ("kIconCloudy", 29, "Mây cụm", "Broken clouds"),
    ("kIconClearNight", 26, "Trời quang", "Clear sky"),
    ("kIconRain", 21, "Mưa nhẹ", "Light rain"),
    ("kIconThunder", 37, "Dông", "Thunderstorm"),
]


def load_colours():
    hdr = (ROOT / "src/ui/theme.h").read_text(encoding="utf-8")
    src = (ROOT / "src/ui/theme.cpp").read_text(encoding="utf-8")
    pat = r"constexpr uint16_t k(\w+) = rgb\((\d+), (\d+), (\d+)\);"
    return {n: q565((int(r), int(g), int(b))) for n, r, g, b in re.findall(pat, hdr + src)}


def q565(c):
    """What the panel shows for an 8-bit colour: RGB565, expanded back to 8 bits."""
    r, g, b = c[0] >> 3, c[1] >> 2, c[2] >> 3
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def rrect(img, box, r, fill):
    x0, y0, x1, y1 = box
    w, h = x1 - x0 + 1, y1 - y0 + 1
    m = Image.new("L", (w * SUP, h * SUP), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, w * SUP - 1, h * SUP - 1), r * SUP, fill=255)
    img.paste(Image.new("RGB", (w, h), fill), (x0, y0), m.resize((w, h), Image.LANCZOS))


def shadow_color(c, ring, spread):
    k = spread + 1 - ring
    alpha = SHADOW_ALPHA * k * k // ((spread + 1) * (spread + 1))
    return q565(tuple(round(b + (i - b) * alpha / 255) for i, b in zip(c["Ink"], c["Background"])))


def shadow(img, c, x, y, w, h):
    """ui::drawShadow: rings from the outside in, shifted down."""
    for i in range(SHADOW_SPREAD, 0, -1):
        rrect(img, (x - i, y + SHADOW_OFFSET_Y - i, x + w - 1 + i, y + SHADOW_OFFSET_Y + h - 1 + i),
              CARD_RADIUS + i, shadow_color(c, i, SHADOW_SPREAD))


def card(img, c, x, y, w, h):
    """ui::drawCardBody: border-coloured rounded rect, background inset by 1 px."""
    rrect(img, (x, y, x + w - 1, y + h - 1), CARD_RADIUS, c["CardBorder"])
    rrect(img, (x + 1, y + 1, x + w - 2, y + h - 2), CARD_RADIUS - 1, c["Background"])


def temp_color(c, t):
    return c["HotColor"] if t >= 36 else c["WarmColor"] if t >= 33 else (
        c["CoolColor"] if t <= 22 else c["Ink"])


def draw_clock(img, c, fonts, hh=10, mm=8, ss=30):
    fx, fy, size = 178, 30, 136
    n, cc = size * SUP, size * SUP / 2
    face = Image.new("RGB", (n, n), c["Background"])
    d = ImageDraw.Draw(face)
    for i in range(FACE_SHADOW_SPREAD, 0, -1):
        r, oy = (66 + i) * SUP, FACE_SHADOW_OFFSET_Y * SUP
        d.ellipse((cc - r, cc + oy - r, cc + r, cc + oy + r),
                  fill=shadow_color(c, i, FACE_SHADOW_SPREAD))
    d.ellipse((cc - 66 * SUP, cc - 66 * SUP, cc + 66 * SUP, cc + 66 * SUP), fill=c["Accent"])
    d.ellipse((cc - 64 * SUP, cc - 64 * SUP, cc + 64 * SUP, cc + 64 * SUP), fill=c["Background"])
    for i in range(60):
        a = math.radians(i * 6)
        sx, sy = math.sin(a), -math.cos(a)
        if i % 5 == 0:
            quarter = i % 15 == 0
            r0 = 50 if quarter else 54
            d.line((cc + sx * r0 * SUP, cc + sy * r0 * SUP, cc + sx * 60 * SUP, cc + sy * 60 * SUP),
                   fill=c["Ink"] if quarter else c["ClockMinor"], width=(4 if quarter else 2) * SUP)
        else:
            px, py, r = cc + sx * 59 * SUP, cc + sy * 59 * SUP, 0.5 * SUP
            d.ellipse((px - r, py - r, px + r, py + r), fill=c["ClockMinor"])

    def hand(deg, length, tail, width, color):
        a = math.radians(deg)
        dx, dy = math.sin(a), -math.cos(a)
        d.line((cc - dx * tail * SUP, cc - dy * tail * SUP, cc + dx * length * SUP,
                cc + dy * length * SUP), fill=color, width=round(width * SUP))

    hand(((hh % 12) + mm / 60) * 30, 32, 0, 6, c["Ink"])
    hand((mm + ss / 60) * 6, 48, 0, 4, c["Ink"])
    hand(ss * 6, 55, 12, 2, c["Highlight"])
    d.ellipse((cc - 4 * SUP, cc - 4 * SUP, cc + 4 * SUP, cc + 4 * SUP), fill=c["Highlight"])
    d.ellipse((cc - SUP, cc - SUP, cc + SUP, cc + SUP), fill=c["Background"])
    img.paste(face.resize((size, size), Image.LANCZOS), (fx, fy))
    ImageDraw.Draw(img).text((fx + size / 2, fy + size / 2 - 40), "12", font=fonts["small"],
                             fill=c["ClockMinor"], anchor="mm")


def render(c, icons, fonts, lang, state):
    t = TEXT[lang]
    icon, temp, desc_vi, desc_en = state
    img = Image.new("RGB", (W, H), c["Background"])
    d = ImageDraw.Draw(img)

    # Header: city, language pill, gear, signal bars (3 of 4 lit)
    d.text((8, 14), t["city"], font=fonts["medium"], fill=c["Ink"], anchor="lm")
    bars_x = 320 - 6 - 4 * 5 + 2
    gear_x = bars_x - 10 - 16
    pill_x = gear_x - 10 - 28
    rrect(img, (pill_x, 5, pill_x + 27, 22), 9, c["Accent"])
    d = ImageDraw.Draw(img)
    d.text((pill_x + 14, 14), t["pill"], font=fonts["small"], fill=c["OnAccent"], anchor="mm")
    img.paste(icons["kIconGear"], (gear_x, 6), icons["kIconGear"])
    for i in range(4):
        h = 4 + i * 3
        d.rectangle((bars_x + i * 5, 20 - h, bars_x + i * 5 + 2, 19),
                    fill=c["Ok"] if i < 3 else c["BarOff"])

    # Card shadows first (details, then weather), then the bodies
    shadow(img, c, 6, 138, 166, 82)
    shadow(img, c, 6, 30, 166, 104)
    shadow(img, c, 180, 168, 132, 50)

    # Weather card
    card(img, c, 6, 30, 166, 104)
    img.paste(icons[icon], (10, 34), icons[icon])
    d = ImageDraw.Draw(img)
    d.text((164, 70), f"{temp}°", font=fonts["temp"], fill=temp_color(c, temp), anchor="rm")
    d.text((14, 120), desc_vi if lang == "vi" else desc_en, font=fonts["medium"],
           fill=c["Accent"], anchor="lm")

    # Details card
    card(img, c, 6, 138, 166, 82)
    d = ImageDraw.Draw(img)
    rows = [("kIconFeels", t["feels"], "39°", temp_color(c, 39)),
            ("kIconHumidity", t["hum"], "74%", c["Ink"]),
            ("kIconWind", t["wind"], "11 km/h", c["Ink"]),
            ("kIconPressure", t["pres"], "1009 hPa", c["Ink"])]
    for i, (ic, label, val, vc) in enumerate(rows):
        top = 139 + i * 20
        mid = top + 10
        if i > 0:
            d.line((14, top, 14 + 149, top), fill=c["Line"])
        img.paste(icons[ic], (14, mid - 8), icons[ic])
        d.text((38, mid), label, font=fonts["small"], fill=c["Muted"], anchor="lm")
        d.text((164, mid), val, font=fonts["small"], fill=vc, anchor="rm")
        if i == 1:
            bx = round(38 + d.textlength(label, font=fonts["small"]) + 6)
            rrect(img, (bx, mid - 2, bx + 29, mid + 1), 2, c["Track"])
            rrect(img, (bx, mid - 2, bx + 22, mid + 1), 2, c["Accent"])
            d = ImageDraw.Draw(img)

    draw_clock(img, c, fonts)

    # Time card: digital readout and date
    card(img, c, 180, 168, 132, 50)
    d = ImageDraw.Draw(img)
    d.text((246, 184), "10:08:30", font=fonts["clock"], fill=c["Ink"], anchor="mm")
    d.text((246, 207), t["date"], font=fonts["small"], fill=c["Accent"], anchor="mm")

    # Footer
    d.text((8, 231), t["age"], font=fonts["small"], fill=c["Muted"], anchor="lm")
    d.text((312, 231), t["tap"], font=fonts["small"], fill=c["Muted"], anchor="rm")

    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = q565(px[x, y])
    return img


# ---------------------------------------------------------------------------
# WiFi screens (screen_wifi.cpp, screen_keyboard.cpp, screen_connect.cpp)
# ---------------------------------------------------------------------------

WIFI_TEXT = {
    "vi": dict(title="WiFi", rescan="Quét lại", connected="Đã kết nối: {}", saved="Đã lưu",
               prev="Trước", next="Sau", space="dấu cách", cancel="Huỷ", connect="Kết nối",
               show="Hiện", failed=("Sai mật khẩu hoặc", "không kết nối được"),
               retry="Nhập lại", forget="Quên mạng"),
    "en": dict(title="WiFi", rescan="Rescan", connected="Connected: {}", saved="Saved",
               prev="Prev", next="Next", space="space", cancel="Cancel", connect="Connect",
               show="Show", failed=("Wrong password or", "network unreachable"),
               retry="Try again", forget="Forget"),
}
# (ssid, rssi, secured, saved, connected)
NETWORKS = [("NhaRieng", -50, True, True, True), ("Cafe_Sua_Da", -62, False, True, False),
            ("VNPT-5G-A1B2", -68, True, False, False), ("Viettel_Home", -80, True, False, False)]
KEY_BG = (236, 243, 252)


def chevron(d, cx, cy, right, color):
    s = 1 if right else -1
    d.line((cx - s * 2, cy - 5, cx + s * 2, cy, cx - s * 2, cy + 5), fill=color, width=2)


def back_header(img, c, fonts, title):
    d = ImageDraw.Draw(img)
    chevron(d, 14, 18, False, c["Ink"])
    d.text((26, 18), title, font=fonts["medium"], fill=c["Ink"], anchor="lm")


def quantise(img):
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = q565(px[x, y])
    return img


def render_wifi(c, fonts, lang):
    t = WIFI_TEXT[lang]
    img = Image.new("RGB", (W, H), c["Background"])
    back_header(img, c, fonts, t["title"])
    rx = 320 - 8 - 92
    rrect(img, (rx, 4, rx + 91, 31), 14, c["Accent"])
    rrect(img, (rx + 1, 5, rx + 90, 30), 13, c["Background"])
    d = ImageDraw.Draw(img)
    d.text((rx + 46, 18), t["rescan"], font=fonts["small"], fill=c["Accent"], anchor="mm")
    d.text((10, 46), t["connected"].format("NhaRieng - 192.168.1.23"), font=fonts["small"],
           fill=c["Ok"], anchor="lm")

    top0 = 60
    shadow(img, c, 6, top0, 308, 144)
    card(img, c, 6, top0, 308, 144)
    d = ImageDraw.Draw(img)
    for i, (ssid, rssi, secured, saved, connected) in enumerate(NETWORKS):
        top, mid = top0 + i * 36, top0 + i * 36 + 18
        if i > 0:
            d.line((16, top, 303, top), fill=c["Line"])
        lit = 1 + sum(rssi >= lvl for lvl in (-75, -65, -55))
        for b in range(4):
            h = 4 + b * 3
            d.rectangle((18 + b * 5, mid + 7 - h, 20 + b * 5, mid + 6),
                        fill=c["Accent"] if b < lit else c["BarOff"])
        d.text((46, mid), ssid, font=fonts["medium"], fill=c["Ink"], anchor="lm")
        if saved and not connected:
            d.text((302, mid), t["saved"], font=fonts["small"], fill=c["Muted"], anchor="rm")
            continue
        if secured:
            d.rounded_rectangle((262, mid - 2, 272, mid + 5), 2, fill=c["Muted"])
            d.arc((263, mid - 7, 271, mid + 1), 180, 360, fill=c["Muted"], width=2)
        if connected:
            d.line((285, mid, 289, mid + 4, 296, mid - 4), fill=c["Ok"], width=2)
        else:
            chevron(d, 294, mid, True, c["Muted"])

    for x0, label, right in ((6, t["prev"], False), (214, t["next"], True)):
        rrect(img, (x0, 210, x0 + 99, 237), 14, c["Track"])
        d = ImageDraw.Draw(img)
        fg = c["ClockMinor"] if not right else c["Ink"]  # first page: Prev disabled
        d.text((x0 + 50, 224), label, font=fonts["small"], fill=fg, anchor="mm")
        chevron(d, x0 + 84 if right else x0 + 16, 224, right, fg)
    d.text((160, 224), "1 / 2", font=fonts["small"], fill=c["Muted"], anchor="mm")
    return quantise(img)


def render_keyboard(c, fonts, lang, layout):
    t = WIFI_TEXT[lang]
    img = Image.new("RGB", (W, H), c["Background"])
    back_header(img, c, fonts, "VNPT-5G-A1B2")
    rrect(img, (6, 34, 313, 67), 10, c["Accent"])
    rrect(img, (7, 35, 312, 66), 9, c["Background"])
    d = ImageDraw.Draw(img)
    pw = "*********"
    d.text((16, 51), pw, font=fonts["medium"], fill=c["Ink"], anchor="lm")
    tw = d.textlength(pw, font=fonts["medium"])
    d.rectangle((17 + tw, 41, 18 + tw, 60), fill=c["Accent"])
    d.text((304, 51), t["show"], font=fonts["small"], fill=c["Accent"], anchor="rm")

    kw, kh, gap, y0 = 29, 36, 3, 74
    full = 10 * kw + 9 * gap
    x0 = (W - full) // 2
    wide = (full - 7 * (kw + gap) - gap) // 2
    rows = (["1234567890", "@#$%&*-+()", "_!?/:;'\"."] if layout == "symbols"
            else ["qwertyuiop", "asdfghjkl", "zxcvbnm"])
    if layout == "upper":
        rows = [r.upper() for r in rows]

    def key(x, y, w, label, accent=False, font="medium"):
        rrect(img, (x, y, x + w - 1, y + kh - 1), 6, c["Accent"] if accent else KEY_BG)
        ImageDraw.Draw(img).text((x + w / 2, y + kh / 2), label, font=fonts[font],
                                 fill=c["OnAccent"] if accent else c["Ink"], anchor="mm")

    def shift_icon(x, w, y, color):
        cx, cy = x + w / 2, y + kh / 2
        ImageDraw.Draw(img).line((cx - 4, cy + 7, cx - 4, cy, cx - 8, cy, cx, cy - 8, cx + 8, cy,
                                  cx + 4, cy, cx + 4, cy + 7, cx - 4, cy + 7), fill=color, width=2)

    def backspace_icon(x, w, y, color):
        cx, cy = x + w / 2, y + kh / 2
        dd = ImageDraw.Draw(img)
        dd.line((cx - 10, cy, cx - 4, cy - 7, cx + 10, cy - 7, cx + 10, cy + 7, cx - 4, cy + 7,
                 cx - 10, cy), fill=color, width=2)
        dd.line((cx - 1, cy - 3, cx + 5, cy + 3), fill=color, width=2)
        dd.line((cx - 1, cy + 3, cx + 5, cy - 3), fill=color, width=2)

    for r in range(2):
        first = x0 + (10 - len(rows[r])) * (kw + gap) // 2
        for i, ch in enumerate(rows[r]):
            key(first + i * (kw + gap), y0 + r * (kh + gap), kw, ch)
    y3 = y0 + 2 * (kh + gap)
    if layout == "symbols":
        first = x0
    else:
        first = x0 + wide + gap
        key(x0, y3, wide, "", accent=layout == "upper")
        shift_icon(x0, wide, y3, c["OnAccent"] if layout == "upper" else c["Ink"])
    for i, ch in enumerate(rows[2]):
        key(first + i * (kw + gap), y3, kw, ch)
    bx = first + len(rows[2]) * (kw + gap)
    key(bx, y3, x0 + full - bx, "")
    backspace_icon(bx, x0 + full - bx, y3, c["Ink"])
    y4 = y3 + kh + gap
    key(x0, y4, wide, "abc" if layout == "symbols" else "123", font="small")
    sx = x0 + wide + gap
    key(sx, y4, 118, t["space"], font="small")
    key(sx + 121, y4, 48, t["cancel"], font="small")
    cx = sx + 121 + 51
    key(cx, y4, x0 + full - cx, t["connect"], accent=True, font="small")
    return quantise(img)


def render_connect_failed(c, fonts, lang):
    t = WIFI_TEXT[lang]
    img = Image.new("RGB", (W, H), c["Background"])
    back_header(img, c, fonts, "VNPT-5G-A1B2")
    shadow(img, c, 30, 70, 260, 110)
    card(img, c, 30, 70, 260, 110)
    d = ImageDraw.Draw(img)
    d.text((160, 98), t["failed"][0], font=fonts["small"], fill=c["Error"], anchor="mm")
    d.text((160, 120), t["failed"][1], font=fonts["small"], fill=c["Error"], anchor="mm")
    rrect(img, (50, 140, 153, 167), 14, c["Accent"])
    rrect(img, (166, 140, 269, 167), 14, c["Accent"])
    rrect(img, (167, 141, 268, 166), 13, c["Background"])
    d = ImageDraw.Draw(img)
    d.text((102, 154), t["retry"], font=fonts["small"], fill=c["OnAccent"], anchor="mm")
    d.text((218, 154), t["forget"], font=fonts["small"], fill=c["Accent"], anchor="mm")
    return quantise(img)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, help="PNG sheet to write")
    parser.add_argument("--scale", type=int, default=2, help="pixel scale (nearest neighbour)")
    parser.add_argument("--lang", choices=["vi", "en"], default="vi")
    parser.add_argument("--screen", choices=["main", "wifi"], default="main",
                        help="main: weather states; wifi: WiFi list, keyboard, connect error")
    args = parser.parse_args()

    icons = {n: gen_icons.render(f, gen_icons.BASE) for n, f in gen_icons.WEATHER_ICONS}
    icons.update({n: gen_icons.render(f, gen_icons.MINI_BASE, gen_icons.MINI_SIZE)
                  for n, f in gen_icons.MINI_ICONS})
    fonts = {
        "small": ImageFont.truetype(str(FONT_DIR / "Roboto-Regular.ttf"), 15),
        "medium": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 20),
        "temp": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 52),
        "clock": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 28),
    }
    colours = load_colours()

    if args.screen == "wifi":
        images = [render_wifi(colours, fonts, args.lang)]
        images += [render_keyboard(colours, fonts, args.lang, lay)
                   for lay in ("lower", "upper", "symbols")]
        images.append(render_connect_failed(colours, fonts, args.lang))
    else:
        images = [render(colours, icons, fonts, args.lang, st) for st in STATES]

    cols, sc, pad = 3, args.scale, 12
    rows = math.ceil(len(images) / cols)
    sheet = Image.new("RGB", (cols * (W * sc + pad) + pad, rows * (H * sc + pad) + pad),
                      (40, 40, 40))
    for i, img in enumerate(images):
        x, y = pad + (i % cols) * (W * sc + pad), pad + (i // cols) * (H * sc + pad)
        sheet.paste(img.resize((W * sc, H * sc), Image.NEAREST), (x, y))
    sheet.save(args.out)
    print(f"preview: {args.out}")


if __name__ == "__main__":
    main()
