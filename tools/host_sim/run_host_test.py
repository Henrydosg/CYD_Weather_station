"""Build and run the TIME FALLS host test (MSVC), then turn its frames into PNG
previews of the whole screen (text column drawn here with Pillow - a
mock of the device layout, not a photo of the device).

Usage (from the project root):
    python tools/host_sim/run_host_test.py [--seeds 6] [--cycles 12] [--no-frames]

Output: build/host_sim/ (exe, PPM frames), previews/timefalls_*.png
"""

import argparse
import glob
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BUILD = ROOT / ".pio" / "host_sim"
PREVIEWS = ROOT / "previews"
SOURCES = [
    "tools/host_sim/host_test.cpp",
    "src/timefalls/sand_sim.cpp",
    "src/timefalls/hourglass_cycle.cpp",
    "src/timefalls/scene_painter.cpp",
    "src/timefalls/sand_scenes.cpp",
    "src/timefalls/air_field.cpp",
]
VSWHERE = r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

SCREEN_W, SCREEN_H = 320, 240
COLUMN_W = 176  # tf::kColumnW: text column left, hourglass region right
SCENE_W, SCENE_H = SCREEN_W - COLUMN_W, SCREEN_H
BG = (7, 17, 29)


def find_vcvars() -> str:
    out = subprocess.run([VSWHERE, "-all", "-products", "*", "-property", "installationPath"],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    for path in (p.strip() for p in out if p.strip()):
        cand = Path(path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if cand.exists():
            return str(cand)
    sys.exit("MSVC (vcvars64.bat) not found")


def build() -> Path:
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / "host_test.exe"
    srcs = " ".join(f'"{ROOT / s}"' for s in SOURCES)
    cmd = (f'call "{find_vcvars()}" >nul && cl /nologo /O2 /EHsc /std:c++17 /W4 /wd4244 '
           f'/I "{ROOT / "include"}" {srcs} /Fe"{exe}" /Fo"{BUILD}\\\\"')
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout, res.stderr)
        sys.exit("host build failed")
    warnings = [l for l in res.stdout.splitlines() if "warning" in l]
    for w in warnings:
        print(w)
    return exe


def compose(frame_png, out_png, progress=0.5, board="cyd28"):
    """Landscape mock: the text column on the left (ClockFace coordinates;
    text by Pillow, sample data) + the real scene frame on the right. No
    other buttons than the Settings gear: tapping the hourglass changes the palette.

    board "cyd35": 480 x 320 like the firmware (src/ui/layout.h): positions x 1.5 /
    y 4/3, font-tied sizes and fonts x 4/3; the scene frame (simulated at the 2.8"
    size) is shown x 1.5 (tf::kViewNum / kViewDen: 3 px cells), cropped to the region.
    """
    import sys as _sys
    from PIL import Image, ImageDraw, ImageFont

    _sys.path.insert(0, str(ROOT / "tools"))
    import gen_icons

    big = board == "cyd35"
    sw, sh = (480, 320) if big else (SCREEN_W, SCREEN_H)
    fx, fy = sw / SCREEN_W, sh / SCREEN_H
    fs = min(fx, fy)

    def X(v):
        return round(v * fx)

    def Y(v):
        return round(v * fy)

    def S(v):
        return round(v * fs)

    column_w = X(COLUMN_W)
    img = Image.new("RGB", (sw, sh), BG)
    scene = Image.open(frame_png).convert("RGB")
    if big:
        scene = scene.resize((scene.width * 3 // 2, scene.height * 3 // 2), Image.NEAREST)
        top = (scene.height - sh) // 2
        scene = scene.crop((0, top, scene.width, top + sh))
    img.paste(scene, (column_w, 0))
    d = ImageDraw.Draw(img)
    fonts = "C:/Windows/Fonts/"
    tfont = ImageFont.truetype(fonts + "Roboto-Bold.ttf", S(68))
    bold = ImageFont.truetype(fonts + "Roboto-Medium.ttf", S(12))
    tiny = ImageFont.truetype(fonts + "Roboto-Regular.ttf", S(12))
    label = ImageFont.truetype(fonts + "Roboto-Regular.ttf", S(13))
    medium = ImageFont.truetype(fonts + "Roboto-Medium.ttf", S(20))
    ink = (243, 235, 221)
    pad = X(12)
    d.text((pad, Y(20) + S(14)), "THỨ BẢY  07/11", font=bold, fill=(199, 167, 107), anchor="ls")
    digit, colon = S(36), S(15)
    slots = [0, digit, 2 * digit, 2 * digit + colon, 3 * digit + colon]
    for i, ch in enumerate("21:38"):  # tight slots
        x = pad + slots[i] + (colon if ch == ":" else digit) // 2
        d.text((x, Y(48) + S(52)), ch, font=tfont, fill=ink, anchor="ms")
    py, pw = Y(112), X(150)
    d.line((pad, py, pad + pw - 1, py), fill=(20, 33, 49))
    d.line((pad, py, pad + int(pw * progress), py), fill=(125, 106, 72))
    ny = Y(120) + S(14)
    d.text((pad, ny), "ÂL 17/9", font=label, fill=(138, 122, 92), anchor="ls")
    x = pad + d.textlength("ÂL 17/9", font=label) + X(8)
    d.text((x, ny), "Còn 82 ngày Tết", font=label, fill=(138, 150, 168), anchor="ls")
    wy = Y(144)
    ic = gen_icons.render(dict(gen_icons.WEATHER_ICONS)["kIconPartlyDay"], gen_icons.BASE, S(30))
    img.paste(ic, (pad, wy + S(4)), ic)
    d.text((pad + S(38), wy + S(24)), "31°", font=medium, fill=ink, anchor="ls")
    x = pad + S(38) + d.textlength("31°", font=medium) + X(8)
    d.text((x, wy + S(24)), "Mây rải rác", font=label, fill=(100, 228, 255), anchor="ls")
    d.text((pad + S(38), wy + S(42)), "TP. Hồ Chí Minh", font=tiny, fill=(138, 150, 168),
           anchor="ls")
    # Settings gear, bottom left (ClockFace::drawGear)
    import math
    gear, gx, gy = (90, 102, 120), X(24), Y(222)
    for i in range(8):
        t = 2 * math.pi * i / 8
        d.line((gx + S(5) * math.cos(t), gy + S(5) * math.sin(t), gx + S(9) * math.cos(t),
                gy + S(9) * math.sin(t)), fill=gear, width=S(3))
    r1, r2 = 6.5 * fs, 2.5 * fs
    d.ellipse((gx - r1, gy - r1, gx + r1, gy + r1), fill=gear)
    d.ellipse((gx - r2, gy - r2, gx + r2, gy + r2), fill=BG)
    img = img.resize((sw * 2, sh * 2), Image.NEAREST)
    img.save(out_png)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=6)
    ap.add_argument("--cycles", type=int, default=12)
    ap.add_argument("--no-frames", action="store_true")
    args = ap.parse_args()

    exe = build()
    frames = BUILD / "frames"
    frames.mkdir(exist_ok=True)
    for f in glob.glob(str(frames / "*")):
        os.remove(f)
    cmd = [str(exe), "--seeds", str(args.seeds), "--cycles", str(args.cycles)]
    if not args.no_frames:
        cmd += ["--frames", str(frames)]
    res = subprocess.run(cmd)

    if not args.no_frames:
        try:
            from PIL import Image
        except ImportError:
            print("Pillow missing: frames left as PPM in", frames)
            sys.exit(res.returncode)
        PREVIEWS.mkdir(exist_ok=True)
        pngs = []
        for ppm in sorted(frames.glob("*.ppm")):
            png = ppm.with_suffix(".png")
            Image.open(ppm).save(png)
            out = PREVIEWS / f"timefalls_{ppm.stem}.png"
            compose(png, out)
            pngs.append(out)
        # contact sheet of the main moments
        pick = ["c01_20s", "c01_59s7", "flip_25", "flip_50", "flip_75", "c02_01s3",
                "c02_30s", "c05_30s", "c10_30s"]
        tiles = [PREVIEWS / f"timefalls_{p}.png" for p in pick if (PREVIEWS / f"timefalls_{p}.png").exists()]
        if tiles:
            cols = 3
            w, h = Image.open(tiles[0]).size
            w //= 2
            h //= 2
            rows = (len(tiles) + cols - 1) // cols
            sheet = Image.new("RGB", (cols * w, rows * h), (0, 0, 0))
            for i, t in enumerate(tiles):
                sheet.paste(Image.open(t).resize((w, h)), ((i % cols) * w, (i // cols) * h))
            sheet.save(PREVIEWS / "timefalls_sheet.png")
            print("previews:", PREVIEWS / "timefalls_sheet.png")
        # 3.5" (480 x 320): the same moments, shown like the cyd35 firmware
        tiles35 = []
        for p in pick:
            png = frames / f"{p}.png"
            if png.exists():
                out = PREVIEWS / f"timefalls35_{p}.png"
                compose(png, out, board="cyd35")
                tiles35.append(out)
        if tiles35:
            cols = 3
            w, h = Image.open(tiles35[0]).size
            w //= 2
            h //= 2
            rows = (len(tiles35) + cols - 1) // cols
            sheet = Image.new("RGB", (cols * w, rows * h), (0, 0, 0))
            for i, t in enumerate(tiles35):
                sheet.paste(Image.open(t).resize((w, h)), ((i % cols) * w, (i // cols) * h))
            sheet.save(PREVIEWS / "timefalls35_sheet.png")
            print("previews:", PREVIEWS / "timefalls35_sheet.png")
        # scene gallery: one row per scene (raw scene region, 2x)
        rows = sorted({p.stem.rsplit("_", 1)[0] for p in frames.glob("scene_*.ppm")})
        if rows:
            cols = sorted({p.stem.rsplit("_", 1)[1] for p in frames.glob("scene_*.ppm")})
            w, h = SCENE_W, SCENE_H
            sheet = Image.new("RGB", (len(cols) * w, len(rows) * h), BG)
            for r, row in enumerate(rows):
                for c, col in enumerate(cols):
                    f = frames / f"{row}_{col}.png"
                    if f.exists():
                        sheet.paste(Image.open(f), (c * w, r * h))
            sheet.save(PREVIEWS / "timefalls_scenes.png")
            print("previews:", PREVIEWS / "timefalls_scenes.png")
    sys.exit(res.returncode)


if __name__ == "__main__":
    main()
