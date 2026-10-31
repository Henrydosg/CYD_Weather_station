"""Generate weather icons as RGB565 + 8-bit alpha arrays for TFT_eSPI.

Icons are drawn with Pillow at a high supersampling factor and downscaled,
which gives smooth anti-aliased edges. On the device they are alpha-blended
onto the background (see src/ui/icon.cpp).

Output: src/ui/assets/icons.h / icons.cpp
Usage (from the project root):
    python tools/gen_icons.py            # write the C++ assets
    python tools/gen_icons.py --preview out.png   # also save a preview sheet
"""

import argparse
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "src" / "ui" / "assets"

BASE = 72        # design grid: all coordinates below are in 0..72 units
SUPER = 8        # supersampling factor
PREVIEW_BG = (22, 33, 58)  # matches the card colour in screen_main.cpp

# Palette
SUN_TOP, SUN_BOTTOM, SUN_RAY = (255, 224, 130), (255, 160, 0), (255, 179, 0)
MOON_TOP, MOON_BOTTOM = (250, 248, 220), (214, 206, 160)
CLOUD_TOP, CLOUD_BOTTOM = (255, 255, 255), (200, 213, 228)
DARK_TOP, DARK_BOTTOM = (160, 172, 190), (96, 108, 128)
RAIN_TOP, RAIN_BOTTOM = (110, 205, 255), (30, 136, 229)
BOLT_TOP, BOLT_BOTTOM = (255, 236, 130), (255, 145, 0)
SNOW = (235, 245, 255)
FOG_TOP, FOG_BOTTOM = (210, 220, 232), (150, 165, 185)


class Canvas:
    def __init__(self, base: int):
        self.base = base
        self.n = base * SUPER
        self.img = Image.new("RGBA", (self.n, self.n), (0, 0, 0, 0))

    def s(self, v: float) -> float:
        return v * SUPER

    def new_mask(self) -> Image.Image:
        return Image.new("L", (self.n, self.n), 0)

    def gradient(self, top, bottom, y0: float, y1: float) -> Image.Image:
        """Vertical gradient between design-unit rows y0..y1."""
        grad = Image.new("RGB", (1, self.n))
        a, b = self.s(y0), self.s(y1)
        for y in range(self.n):
            t = min(1.0, max(0.0, (y - a) / max(1.0, b - a)))
            grad.putpixel((0, y), tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
        return grad.resize((self.n, self.n))

    def fill(self, mask: Image.Image, top, bottom=None, y0=0.0, y1=None):
        bottom = bottom or top
        layer = self.gradient(top, bottom, y0, y1 if y1 is not None else self.base).convert("RGBA")
        layer.putalpha(mask)
        self.img = Image.alpha_composite(self.img, layer)

    def erase(self, mask: Image.Image, grow: float):
        """Cut a transparent gap around a shape so overlapping parts separate."""
        size = int(self.s(grow)) * 2 + 1
        grown = mask.filter(ImageFilter.MaxFilter(size))
        alpha = ImageChops.subtract(self.img.getchannel("A"), grown)
        self.img.putalpha(alpha)

    # -- shape masks (design units) ------------------------------------------

    def circle_mask(self, cx, cy, r, mask=None):
        mask = mask or self.new_mask()
        ImageDraw.Draw(mask).ellipse(
            [self.s(cx - r), self.s(cy - r), self.s(cx + r), self.s(cy + r)], fill=255)
        return mask

    def line_mask(self, x0, y0, x1, y1, width, mask=None):
        """Line with round caps."""
        mask = mask or self.new_mask()
        d = ImageDraw.Draw(mask)
        d.line([self.s(x0), self.s(y0), self.s(x1), self.s(y1)], fill=255, width=int(self.s(width)))
        for x, y in ((x0, y0), (x1, y1)):
            self.circle_mask(x, y, width / 2, mask)
        return mask

    def to_size(self) -> Image.Image:
        return self.img.resize((self.base, self.base), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------

def cloud_mask(c: Canvas, cx, cy, k=1.0):
    m = c.new_mask()
    c.circle_mask(cx - 14 * k, cy + 2 * k, 10 * k, m)
    c.circle_mask(cx + 1 * k, cy - 6 * k, 14 * k, m)
    c.circle_mask(cx + 15 * k, cy + 1 * k, 11 * k, m)
    ImageDraw.Draw(m).rounded_rectangle(
        [c.s(cx - 24 * k), c.s(cy + 1 * k), c.s(cx + 26 * k), c.s(cy + 12 * k)],
        radius=c.s(5.5 * k), fill=255)
    return m


def draw_cloud(c: Canvas, cx, cy, k=1.0, dark=False, gap=True):
    m = cloud_mask(c, cx, cy, k)
    if gap:
        c.erase(m, 2.0)
    top, bottom = (DARK_TOP, DARK_BOTTOM) if dark else (CLOUD_TOP, CLOUD_BOTTOM)
    c.fill(m, top, bottom, cy - 20 * k, cy + 12 * k)


def draw_sun(c: Canvas, cx, cy, r):
    rays = c.new_mask()
    for i in range(8):
        a = i * math.pi / 4
        c.line_mask(cx + math.cos(a) * (r + 5), cy + math.sin(a) * (r + 5),
                    cx + math.cos(a) * (r + 10.5), cy + math.sin(a) * (r + 10.5), 3.4, rays)
    c.fill(rays, SUN_RAY)
    c.fill(c.circle_mask(cx, cy, r), SUN_TOP, SUN_BOTTOM, cy - r, cy + r)


def draw_moon(c: Canvas, cx, cy, r, stars=True):
    disc = c.circle_mask(cx, cy, r)
    cut = c.circle_mask(cx + r * 0.55, cy - r * 0.4, r * 0.82)
    c.fill(ImageChops.subtract(disc, cut), MOON_TOP, MOON_BOTTOM, cy - r, cy + r)
    if stars:
        for sx, sy, sr in ((cx + r * 0.9, cy - r * 1.1, 2.6), (cx + r * 1.45, cy - r * 0.2, 1.8)):
            m = c.new_mask()
            d = ImageDraw.Draw(m)
            pts = []
            for i in range(8):
                rad = sr if i % 2 == 0 else sr * 0.35
                a = i * math.pi / 4 - math.pi / 2
                pts.append((c.s(sx + math.cos(a) * rad), c.s(sy + math.sin(a) * rad)))
            d.polygon(pts, fill=255)
            c.fill(m, MOON_TOP)


def draw_drops(c: Canvas, xs, y0, length, width):
    m = c.new_mask()
    for i, x in enumerate(xs):
        y = y0 + (i % 2) * 4
        c.line_mask(x, y, x - length * 0.35, y + length, width, m)
    c.fill(m, RAIN_TOP, RAIN_BOTTOM, y0, y0 + length + 4)


def draw_flakes(c: Canvas, centers, r):
    m = c.new_mask()
    for x, y in centers:
        for i in range(3):
            a = i * math.pi / 3
            c.line_mask(x - math.cos(a) * r, y - math.sin(a) * r,
                        x + math.cos(a) * r, y + math.sin(a) * r, 1.8, m)
    c.fill(m, SNOW)


def draw_bolt(c: Canvas, ox, oy):
    pts = [(40, 0), (27, 19), (35, 19), (29, 34), (46, 13), (37, 13), (44, 0)]
    m = c.new_mask()
    ImageDraw.Draw(m).polygon([(c.s(ox + x - 36), c.s(oy + y)) for x, y in pts], fill=255)
    c.erase(m, 1.5)
    c.fill(m, BOLT_TOP, BOLT_BOTTOM, oy, oy + 34)


# ---------------------------------------------------------------------------
# Weather icons (72 x 72)
# ---------------------------------------------------------------------------

def icon_clear_day(c):
    draw_sun(c, 36, 36, 15)


def icon_clear_night(c):
    draw_moon(c, 33, 38, 20)


def icon_partly_day(c):
    draw_sun(c, 27, 25, 11)
    draw_cloud(c, 40, 42, 0.95)


def icon_partly_night(c):
    draw_moon(c, 27, 26, 13, stars=False)
    draw_cloud(c, 40, 42, 0.95)


def icon_cloudy(c):
    draw_cloud(c, 44, 28, 0.75, dark=True, gap=False)
    draw_cloud(c, 33, 41, 1.0)


def icon_fog(c):
    draw_cloud(c, 36, 26, 0.9, gap=False)
    m = c.new_mask()
    d = ImageDraw.Draw(m)
    for i, (x0, x1) in enumerate(((12, 52), (20, 62), (14, 48))):
        y = 45 + i * 8
        d.rounded_rectangle([c.s(x0), c.s(y), c.s(x1), c.s(y + 4.2)], radius=c.s(2.1), fill=255)
    c.erase(m, 1.5)
    c.fill(m, FOG_TOP, FOG_BOTTOM, 44, 66)


def icon_drizzle(c):
    draw_cloud(c, 36, 28, 1.0, gap=False)
    draw_drops(c, (24, 36, 48), 48, 7, 3.0)


def icon_rain(c):
    draw_cloud(c, 36, 26, 1.0, dark=True, gap=False)
    draw_drops(c, (20, 31, 42, 53), 45, 13, 3.4)


def icon_snow(c):
    draw_cloud(c, 36, 26, 1.0, gap=False)
    draw_flakes(c, ((22, 52), (36, 58), (50, 52)), 5)


def icon_thunder(c):
    draw_cloud(c, 36, 24, 1.0, dark=True, gap=False)
    draw_drops(c, (21, 53), 44, 10, 3.0)
    draw_bolt(c, 37, 34)


WEATHER_ICONS = [
    ("kIconClearDay", icon_clear_day),
    ("kIconClearNight", icon_clear_night),
    ("kIconPartlyDay", icon_partly_day),
    ("kIconPartlyNight", icon_partly_night),
    ("kIconCloudy", icon_cloudy),
    ("kIconFog", icon_fog),
    ("kIconDrizzle", icon_drizzle),
    ("kIconRain", icon_rain),
    ("kIconSnow", icon_snow),
    ("kIconThunder", icon_thunder),
]

# ---------------------------------------------------------------------------
# Small detail icons (20 x 20 design grid)
# ---------------------------------------------------------------------------

def mini_humidity(c):
    m = c.new_mask()
    c.circle_mask(10, 12.5, 5.5, m)
    ImageDraw.Draw(m).polygon(
        [(c.s(10), c.s(2)), (c.s(4.9), c.s(10.5)), (c.s(15.1), c.s(10.5))], fill=255)
    c.fill(m, RAIN_TOP, RAIN_BOTTOM, 2, 18)


def mini_wind(c):
    m = c.new_mask()
    d = ImageDraw.Draw(m)
    w = int(c.s(2.0))
    # Top stroke curls upwards, middle is straight, bottom curls downwards.
    # Pillow arcs run clockwise from `start` to `end` (0 = 3 o'clock).
    c.line_mask(2, 7, 12, 7, 2.0, m)
    d.arc([c.s(9.5), c.s(2), c.s(14.5), c.s(7)], start=180, end=450, fill=255, width=w)
    c.line_mask(2, 11, 17, 11, 2.0, m)
    c.line_mask(2, 15, 10, 15, 2.0, m)
    d.arc([c.s(7.5), c.s(15), c.s(12.5), c.s(20) - 1], start=270, end=540, fill=255, width=w)
    c.fill(m, (200, 230, 255), (140, 190, 230), 2, 18)


def mini_pressure(c):
    m = c.new_mask()
    d = ImageDraw.Draw(m)
    d.ellipse([c.s(2), c.s(2), c.s(18), c.s(18)], outline=255, width=int(c.s(2.0)))
    c.line_mask(10, 10, 14, 6, 2.0, m)
    c.circle_mask(10, 10, 2.0, m)
    c.fill(m, (255, 210, 140), (240, 150, 60), 2, 18)


def mini_feels(c):
    m = c.new_mask()
    c.line_mask(10, 3.5, 10, 12, 4.4, m)
    c.circle_mask(10, 14.5, 4.2, m)
    c.fill(m, (255, 150, 120), (235, 80, 70), 2, 19)


def mini_gear(c):
    m = c.new_mask()
    d = ImageDraw.Draw(m)
    teeth = 8
    for i in range(teeth):
        a = i * 2 * math.pi / teeth
        c.line_mask(10 + math.cos(a) * 5, 10 + math.sin(a) * 5,
                    10 + math.cos(a) * 8, 10 + math.sin(a) * 8, 3.2, m)
    c.circle_mask(10, 10, 6.2, m)
    hole = c.circle_mask(10, 10, 2.6)
    d.bitmap((0, 0), hole, fill=0)
    c.fill(m, (255, 255, 255), (215, 228, 245), 2, 18)


MINI_ICONS = [
    ("kIconGear", mini_gear),
    ("kIconFeels", mini_feels),
    ("kIconHumidity", mini_humidity),
    ("kIconWind", mini_wind),
    ("kIconPressure", mini_pressure),
]
MINI_BASE = 20   # design grid of the detail icons
MINI_SIZE = 16   # output size (one detail row is 20 px tall)


def render(fn, base, size=None) -> Image.Image:
    c = Canvas(base)
    fn(c)
    img = c.to_size()
    return img if size is None or size == base else c.img.resize((size, size), Image.LANCZOS)


def to_rgb565_alpha(img: Image.Image):
    rgb, alpha = [], []
    channels = [img.getchannel(ch).tobytes() for ch in "RGBA"]
    for r, g, b, a in zip(*channels):
        alpha.append(a)
        rgb.append(0 if a == 0 else ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return rgb, alpha


def c_array(ctype, name, values, fmt, per_line):
    lines = [f"static const {ctype} {name}[] PROGMEM = {{"]
    for i in range(0, len(values), per_line):
        lines.append("  " + ", ".join(fmt.format(v) for v in values[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", help="save a PNG preview sheet to this path")
    args = parser.parse_args()

    images = [(n, render(f, BASE)) for n, f in WEATHER_ICONS]
    images += [(n, render(f, MINI_BASE, MINI_SIZE)) for n, f in MINI_ICONS]

    header = [
        "// Generated by tools/gen_icons.py - do not edit.",
        "#pragma once",
        "",
        '#include "../icon.h"',
        "",
    ]
    source = [
        "// Generated by tools/gen_icons.py - do not edit.",
        '#include "icons.h"',
        "",
    ]
    total = 0
    for name, img in images:
        rgb, alpha = to_rgb565_alpha(img)
        header.append(f"extern const Icon {name};  // {img.width}x{img.height}")
        source.append(c_array("uint16_t", f"{name}Rgb", rgb, "0x{:04X}", 12))
        source.append(c_array("uint8_t", f"{name}Alpha", alpha, "0x{:02X}", 20))
        source.append(f"const Icon {name} = {{{img.width}, {img.height}, {name}Rgb, {name}Alpha}};")
        source.append("")
        total += len(rgb) * 3

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "icons.h").write_text("\n".join(header) + "\n", encoding="utf-8", newline="\n")
    (OUT_DIR / "icons.cpp").write_text("\n".join(source), encoding="utf-8", newline="\n")
    print(f"{len(images)} icons, {total} bytes")

    if args.preview:
        scale = 2
        cell = BASE * scale + 16
        sheet = Image.new("RGB", (cell * 5, cell * 3), PREVIEW_BG)
        for i, (_, img) in enumerate(images):
            big = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
            x, y = (i % 5) * cell + 8, (i // 5) * cell + 8
            sheet.paste(big, (x, y), big)
        sheet.save(args.preview)
        print(f"preview: {args.preview}")


if __name__ == "__main__":
    main()
