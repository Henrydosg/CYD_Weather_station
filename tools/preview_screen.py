"""Render PC previews of the main screen for every sky palette.

Mirrors src/ui/screen_main.cpp, analog_clock.cpp and draw_utils.cpp (same
layout, Roboto fonts and icons from gen_icons.py), reads the palettes and glass
alphas from src/ui/theme.cpp / theme.h, and quantises to RGB565 like the panel.
Panel gamma and viewing angle are not simulated.

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
SKY_MID_ROW = 120
CARD_RADIUS = 10

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
MUTED = (210, 225, 244)
OK = (120, 235, 160)
BAR_FILL = (110, 205, 255)
SECOND_HAND = (255, 184, 77)

TEXT = {
    "vi": dict(city="TP. Hồ Chí Minh", feels="Cảm giác", hum="Độ ẩm", wind="Gió",
               pres="Áp suất", age="5 phút trước", tap="Chạm để làm mới",
               date="Thứ Bảy, 31/10", pill="VI",
               desc={"Clear": "Trời quang", "Cloudy": "Mây cụm", "Rain": "Mưa nhẹ",
                     "Storm": "Dông"}),
    "en": dict(city="Ho Chi Minh City", feels="Feels like", hum="Humidity", wind="Wind",
               pres="Pressure", age="5 min ago", tap="Tap to refresh",
               date="Saturday, 31/10", pill="EN",
               desc={"Clear": "Clear sky", "Cloudy": "Broken clouds", "Rain": "Light rain",
                     "Storm": "Thunderstorm"}),
}
FAMILY_ICON = {"Clear": ("kIconClearDay", "kIconClearNight"), "Cloudy": ("kIconCloudy",) * 2,
               "Rain": ("kIconRain",) * 2, "Storm": ("kIconThunder",) * 2}
PHASES = ["Dawn", "Day", "Dusk", "Night"]


def load_theme():
    src = (ROOT / "src/ui/theme.cpp").read_text(encoding="utf-8")
    rgb = r"rgb\((\d+), (\d+), (\d+)\)"
    palettes = {}
    for m in re.finditer(rf"constexpr Palette k(\w+) = \{{{rgb}, {rgb}, {rgb}, {rgb}\}};", src):
        v = [int(x) for x in m.groups()[1:]]
        palettes[m.group(1)] = [tuple(v[i:i + 3]) for i in range(0, 12, 3)]
    hdr = (ROOT / "src/ui/theme.h").read_text(encoding="utf-8")
    alphas = {k: int(v) for k, v in re.findall(r"constexpr uint8_t k(\w+Alpha) = (\d+);", hdr)}
    return palettes, alphas


def q565(c):
    return (c[0] & 0xF8, c[1] & 0xFC, c[2] & 0xF8)


def blend(alpha, fg, bg):
    return tuple(round(bg[i] + (fg[i] - bg[i]) * alpha / 255) for i in range(3))


class Screen:
    def __init__(self, pal, alphas):
        self.top, self.mid, self.bottom, self.accent = pal
        self.a = alphas
        self.img = Image.new("RGB", (W, H))
        self.px = self.img.load()
        for y in range(H):
            self.hline(0, y, W, self.sky(y))

    def sky(self, y):
        y = max(0, min(H - 1, y))
        if y <= SKY_MID_ROW:
            return blend(y * 255 // SKY_MID_ROW, self.mid, self.top)
        return blend((y - SKY_MID_ROW) * 255 // (H - 1 - SKY_MID_ROW), self.bottom, self.mid)

    def glass(self, y, alpha=None):
        return blend(self.a["GlassAlpha"] if alpha is None else alpha, BLACK, self.sky(y))

    def hline(self, x, y, w, c):
        for i in range(x, x + w):
            self.px[i, y] = c

    def draw_glass(self, x, y, w, h, r, alpha, rim, row_begin=0):
        """Same algorithm as ui::drawGlass."""
        for j in range(row_begin, h):
            sy = y + j
            sky = self.sky(sy)
            g = blend(alpha, BLACK, sky)
            lit = blend(self.a["RimAlpha"], WHITE, g)
            dy = r - (j + 0.5) if j < r else ((j + 0.5) - (h - r) if j >= h - r else 0)
            inset = 0
            if dy > 0:
                edge = lit if rim and j < r else g
                for i in range(r):
                    dx = r - (i + 0.5)
                    cover = r + 0.5 - math.hypot(dx, dy)
                    if cover >= 1:
                        break
                    c = sky if cover <= 0 else blend(int(cover * 255), edge, sky)
                    self.px[x + i, sy] = c
                    self.px[x + w - 1 - i, sy] = c
                    inset = i + 1
            self.hline(x + inset, sy, w - 2 * inset, lit if rim and j == 0 else g)

    def card(self, x, y, w, h):
        self.draw_glass(x, y, w, h, CARD_RADIUS, self.a["GlassAlpha"], True)

    def icon(self, name, x, y):
        im = ICONS[name]
        self.img.paste(im, (x, y), im)


def text(img, xy, s, font, fill, anchor):
    ImageDraw.Draw(img).text(xy, s, font=font, fill=fill, anchor=anchor)


def temp_color(c):
    return (255, 110, 90) if c >= 36 else (255, 170, 70) if c >= 33 else (
        (140, 210, 255) if c <= 22 else WHITE)


def draw_clock(s, fonts, hh=10, mm=8, ss=30):
    """Face at (178, 30), 136 px: ring, glass disc, ticks and hands."""
    fx, fy, size, sup = 178, 30, 136, 4
    n = size * sup
    face = s.img.crop((fx, fy, fx + size, fy + size)).resize((n, n), Image.NEAREST)
    d = ImageDraw.Draw(face)
    c = n / 2
    mid_glass = s.glass(fy + size // 2)
    ring = blend(s.a["RingAlpha"], WHITE, mid_glass)
    d.ellipse((c - 66 * sup, c - 66 * sup, c + 66 * sup, c + 66 * sup), fill=ring)
    disc = Image.new("L", (n, n), 0)
    ImageDraw.Draw(disc).ellipse((c - 64 * sup, c - 64 * sup, c + 64 * sup, c + 64 * sup), fill=255)
    rows = Image.new("RGB", (1, n))
    for y in range(n):
        rows.putpixel((0, y), s.glass(fy + y // sup))
    face.paste(rows.resize((n, n)), (0, 0), disc)
    minor = blend(s.a["MinorTickAlpha"], MUTED, mid_glass)
    for i in range(60):
        a = math.radians(i * 6)
        sx, sy = math.sin(a), -math.cos(a)
        if i % 5 == 0:
            r0 = 50 if i % 15 == 0 else 54
            d.line((c + sx * r0 * sup, c + sy * r0 * sup, c + sx * 60 * sup, c + sy * 60 * sup),
                   fill=WHITE if i % 15 == 0 else minor, width=(3 if i % 15 == 0 else 2) * sup)
        else:
            px, py, r = c + sx * 59 * sup, c + sy * 59 * sup, 0.6 * sup
            d.ellipse((px - r, py - r, px + r, py + r), fill=minor)

    def hand(deg, length, tail, width, color):
        a = math.radians(deg)
        dx, dy = math.sin(a), -math.cos(a)
        d.line((c - dx * tail * sup, c - dy * tail * sup, c + dx * length * sup,
                c + dy * length * sup), fill=color, width=round(width * sup))

    hand(((hh % 12) + mm / 60) * 30, 32, 0, 5, WHITE)
    hand((mm + ss / 60) * 6, 48, 0, 3.6, WHITE)
    hand(ss * 6, 55, 12, 1.8, SECOND_HAND)
    d.ellipse((c - 4 * sup, c - 4 * sup, c + 4 * sup, c + 4 * sup), fill=SECOND_HAND)
    d.ellipse((c - sup, c - sup, c + sup, c + sup), fill=mid_glass)
    s.img.paste(face.resize((size, size), Image.LANCZOS), (fx, fy))
    text(s.img, (fx + size / 2, fy + size / 2 - 40), "12", fonts["small"], MUTED, "mm")


def render(pal, alphas, family, phase, lang, fonts):
    t = TEXT[lang]
    s = Screen(pal, alphas)
    img = s.img

    # Header on the sky: city, language pill, gear, signal bars.
    text(img, (8, 14), t["city"], fonts["medium"], WHITE, "lm")
    bars_x = 320 - 6 - 4 * 5 + 2
    gear_x = bars_x - 10 - 16
    pill_x = gear_x - 10 - 28
    ImageDraw.Draw(img).rounded_rectangle((pill_x, 5, pill_x + 27, 22), 9, fill=s.glass(14))
    text(img, (pill_x + 14, 14), t["pill"], fonts["small"], WHITE, "mm")
    s.icon("kIconGear", gear_x, 6)
    for i in range(4):
        h = 4 + i * 3
        ImageDraw.Draw(img).rectangle((bars_x + i * 5, 20 - h, bars_x + i * 5 + 2, 19),
                                      fill=OK if i < 3 else blend(50, WHITE, s.sky(20)))

    # Weather card
    s.card(6, 30, 166, 104)
    night = phase == "Night"
    s.icon(FAMILY_ICON[family][1 if night else 0], 10, 34)
    text(img, (164, 70), "33°", fonts["temp"], temp_color(33), "rm")
    text(img, (14, 120), t["desc"][family], fonts["medium"], s.accent, "lm")

    # Details card
    dy = 138
    s.card(6, dy, 166, 82)
    rows = [("kIconFeels", t["feels"], "39°", temp_color(39)), ("kIconHumidity", t["hum"], "74%", WHITE),
            ("kIconWind", t["wind"], "11 km/h", WHITE), ("kIconPressure", t["pres"], "1009 hPa", WHITE)]
    for i, (ic, label, val, vc) in enumerate(rows):
        top = dy + 1 + i * 20
        mid = top + 10
        dr = ImageDraw.Draw(img)
        if i > 0:
            dr.line((14, top, 14 + 149, top), fill=blend(s.a["LineAlpha"], WHITE, s.glass(top)))
        s.icon(ic, 14, mid - 8)
        text(img, (38, mid), label, fonts["small"], MUTED, "lm")
        text(img, (164, mid), val, fonts["small"], vc, "rm")
        if i == 1:
            bx = round(38 + dr.textlength(label, font=fonts["small"]) + 6)
            dr.rounded_rectangle((bx, mid - 2, bx + 29, mid + 1), 2,
                                 fill=blend(s.a["TrackAlpha"], WHITE, s.glass(mid)))
            dr.rounded_rectangle((bx, mid - 2, bx + 22, mid + 1), 2, fill=BAR_FILL)

    draw_clock(s, fonts)

    # Time card: digital readout (sprite) and date
    s.card(180, 168, 132, 50)
    text(img, (246, 184), "10:08:30", fonts["clock"], WHITE, "mm")
    text(img, (246, 207), t["date"], fonts["small"], MUTED, "mm")

    # Footer strip
    s.draw_glass(0, 222, W, H - 222, 0, s.a["FooterGlassAlpha"], False)
    text(img, (8, 231), t["age"], fonts["small"], MUTED, "lm")
    text(img, (312, 231), t["tap"], fonts["small"], MUTED, "rm")

    return img


def quantise(img):
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            px[x, y] = q565(px[x, y])
    return img


ICONS = {}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, help="PNG sheet to write")
    parser.add_argument("--scale", type=int, default=1, help="pixel scale (nearest neighbour)")
    parser.add_argument("--lang", choices=["vi", "en"], default="vi")
    args = parser.parse_args()

    ICONS.update({n: gen_icons.render(f, gen_icons.BASE) for n, f in gen_icons.WEATHER_ICONS})
    ICONS.update({n: gen_icons.render(f, gen_icons.MINI_BASE, gen_icons.MINI_SIZE)
                  for n, f in gen_icons.MINI_ICONS})
    fonts = {
        "small": ImageFont.truetype(str(FONT_DIR / "Roboto-Regular.ttf"), 15),
        "medium": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 20),
        "temp": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 46),
        "clock": ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 26),
    }
    palettes, alphas = load_theme()
    families = ["Clear", "Cloudy", "Rain", "Storm"]

    sc, pad, label_h = args.scale, 12, 22
    cw, ch = W * sc + pad, H * sc + pad + label_h
    sheet = Image.new("RGB", (len(PHASES) * cw + pad, len(families) * ch + pad), (30, 30, 30))
    label_font = ImageFont.truetype(str(FONT_DIR / "Roboto-Medium.ttf"), 15)
    for r, fam in enumerate(families):
        for c, phase in enumerate(PHASES):
            pal = palettes[fam + phase]
            img = quantise(render(pal, alphas, fam, phase, args.lang, fonts))
            x, y = pad + c * cw, pad + r * ch
            ImageDraw.Draw(sheet).text((x, y), f"k{fam}{phase}", font=label_font, fill=(230, 230, 230))
            sheet.paste(img.resize((W * sc, H * sc), Image.NEAREST), (x, y + label_h))
    sheet.save(args.out)
    print(f"preview: {args.out}")


if __name__ == "__main__":
    main()
