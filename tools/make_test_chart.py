"""Write a 320x240 baseline JPEG test chart for the photo viewer.

Copy it to /photos on the SD card and compare the panel with the PNG preview:
- colour bars: red / green / blue / cyan / magenta / yellow / white / black
  (a bar showing its opposite colour means the image is inverted),
- 8-step grey ramp with the 0..255 level printed (steps that merge or turn
  dark at the bright end mean the panel gamma is off),
- smooth grey and hue gradients (banding is normal for RGB565).

Usage (from the project root, needs Pillow):
    python tools/make_test_chart.py --out previews/00_test_chart.jpg
"""

import argparse
import colorsys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H = 320, 240
FONT = Path("C:/Windows/Fonts/Roboto-Medium.ttf")
BARS = [("R", (255, 0, 0)), ("G", (0, 255, 0)), ("B", (0, 0, 255)), ("C", (0, 255, 255)),
        ("M", (255, 0, 255)), ("Y", (255, 255, 0)), ("W", (255, 255, 255)), ("K", (0, 0, 0))]
STEPS = 8


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True, help="JPEG to write")
    args = parser.parse_args()

    img = Image.new("RGB", (W, H), (128, 128, 128))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(str(FONT), 12)

    # Colour bars (y 0..79)
    bw = W // len(BARS)
    for i, (name, rgb) in enumerate(BARS):
        d.rectangle((i * bw, 0, (i + 1) * bw - 1, 79), fill=rgb)
        ink = (0, 0, 0) if sum(rgb) > 380 else (255, 255, 255)
        d.text((i * bw + bw // 2, 40), name, font=font, fill=ink, anchor="mm")

    # Grey ramp with levels (y 80..149)
    sw = W // STEPS
    for i in range(STEPS):
        v = round(i * 255 / (STEPS - 1))
        d.rectangle((i * sw, 80, (i + 1) * sw - 1, 149), fill=(v, v, v))
        ink = (0, 0, 0) if v > 127 else (255, 255, 255)
        d.text((i * sw + sw // 2, 115), str(v), font=font, fill=ink, anchor="mm")

    # Smooth grey gradient (y 150..194) and hue gradient (y 195..239)
    for x in range(W):
        v = round(x * 255 / (W - 1))
        d.line((x, 150, x, 194), fill=(v, v, v))
        r, g, b = colorsys.hsv_to_rgb(x / W, 1.0, 1.0)
        d.line((x, 195, x, 239), fill=(round(r * 255), round(g * 255), round(b * 255)))

    out = Path(args.out)
    img.save(out, "JPEG", quality=95, progressive=False)
    img.save(out.with_suffix(".png"))
    print(f"test chart: {out} (+ {out.with_suffix('.png').name} for comparison)")


if __name__ == "__main__":
    main()
