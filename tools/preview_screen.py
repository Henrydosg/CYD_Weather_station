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


def card(img, c, x, y, w, h):
    """ui::drawCard: border-coloured rounded rect, background inset by 1 px."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, help="PNG sheet to write")
    parser.add_argument("--scale", type=int, default=2, help="pixel scale (nearest neighbour)")
    parser.add_argument("--lang", choices=["vi", "en"], default="vi")
    args = parser.parse_args()

    icons = {n: gen_icons.render(f, gen_icons.BASE) for n, f in gen_icons.WEATHER_ICONS}
    icons.update({n: gen_icons.render(f, gen_icons.MINI_BASE, gen_icons.MINI_SIZE)
                  for n, f in gen_icons.MINI_ICONS})
    fonts = {
        "small": ImageFont.truetype(str(FONT_DIR / "Roboto-Regular.ttf"), 15),
        "medium": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 20),
        "temp": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 46),
        "clock": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 26),
    }
    colours = load_colours()

    cols, sc, pad = 3, args.scale, 12
    rows = math.ceil(len(STATES) / cols)
    sheet = Image.new("RGB", (cols * (W * sc + pad) + pad, rows * (H * sc + pad) + pad),
                      (40, 40, 40))
    for i, state in enumerate(STATES):
        img = render(colours, icons, fonts, args.lang, state)
        x, y = pad + (i % cols) * (W * sc + pad), pad + (i // cols) * (H * sc + pad)
        sheet.paste(img.resize((W * sc, H * sc), Image.NEAREST), (x, y))
    sheet.save(args.out)
    print(f"preview: {args.out}")


if __name__ == "__main__":
    main()
