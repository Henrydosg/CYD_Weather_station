"""Build and publish a firmware release to docs/ (GitHub Pages), one per board.

Boards (BOARDS below; each matches include/boards/<name>.h):
  cyd28  ESP32-2432S028R 2.8"  env cyd-release    ota/latest.json  manifest.json
  cyd35  ESP32-3248S035R 3.5"  env cyd35-release  ota/cyd35.json   manifest-cyd35.json
The 2.8" keeps ota/latest.json and manifest.json, which devices and links in use
already read.

Steps, for every board:
1. Reads config::kFirmwareVersion and CYD_PAGES_URL from include/config.h, and
   checks BOARD_OTA_MANIFEST in include/boards/<name>.h against BOARDS.
2. Builds the release env (no include/secrets.h inside).
3. Checks that the WiFi SSID / password of include/secrets.h do not appear in
   the binary (prints only OK / FAILED, never the values).
4. Writes, under docs/ (or --out):
     firmware/<prefix>-<version>.bin     app image (OTA and web installer)
     firmware/bootloader.bin, partitions.bin, boot_app0.bin   (web installer,
                                         shared: same chip and partition table)
     ota/<manifest>                      {"version", "url", "size", "md5"}
     <web manifest>                      ESP Web Tools manifest (docs/index.html)
   Older <prefix>-*.bin files are removed (git history keeps them).

Then commit and push docs/; devices see the update within a day, or at once
with Settings -> Firmware -> Check. GitHub Pages must serve the main branch
/docs folder (repository Settings -> Pages).

Usage (from the project root):
    python tools/release.py                 build and write every board to docs/
    python tools/release.py --board cyd35   only one board (the others stay as they are)
    python tools/release.py --no-build      reuse .pio/build/<env>
    python tools/release.py --out DIR       write to DIR instead of docs/ (a dry run)
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
PIO = Path(os.environ.get("USERPROFILE", str(Path.home()))) / ".platformio"
BOOT_APP0 = PIO / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"
# Flash offsets of the parts (ESP32, min_spiffs.csv partition table)
OFFSETS = {"bootloader.bin": 0x1000, "partitions.bin": 0x8000, "boot_app0.bin": 0xE000,
           "app": 0x10000}
SHARED_PARTS = ("bootloader.bin", "partitions.bin")

# name, release env, OTA manifest (docs/ota/), app file prefix, web manifest, title
BOARDS = [
    ("cyd28", "cyd-release", "latest.json", "cyd-weather", "manifest.json",
     'CYD Weather Station 2.8"'),
    ("cyd35", "cyd35-release", "cyd35.json", "cyd35-weather", "manifest-cyd35.json",
     'CYD Weather Station 3.5"'),
]


def read_text(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def config_string(name):
    m = re.search(name + r'\[\]\s*=\s*\s*"([^"]+)"', read_text("include/config.h"))
    if not m:
        sys.exit(f"{name} not found in include/config.h")
    return m.group(1)


def define_string(rel, name):
    m = re.search(r'#define\s+' + name + r'\s+"([^"]+)"', read_text(rel))
    if not m:
        sys.exit(f"#define {name} not found in {rel}")
    return m.group(1)


def secrets_strings():
    path = ROOT / "include" / "secrets.h"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    return [v for v in re.findall(r'k\w+\[\]\s*=\s*"([^"]*)"', text) if len(v) >= 4]


def build(env):
    pio = PIO / "penv" / "Scripts" / "pio.exe"
    cmd = [str(pio) if pio.exists() else "pio", "run", "-e", env]
    if subprocess.run(cmd, cwd=ROOT).returncode != 0:
        sys.exit(f"build of {env} failed")


def release_board(board, version, base_url, out, no_build, secrets):
    name, env, ota_name, prefix, web_name, title = board
    in_header = define_string(f"include/boards/{name}.h", "BOARD_OTA_MANIFEST")
    if in_header != ota_name:
        sys.exit(f"{name}: BOARD_OTA_MANIFEST is {in_header}, release.py expects {ota_name}")
    if not no_build:
        build(env)
    build_dir = ROOT / ".pio" / "build" / env
    app = (build_dir / "firmware.bin").read_bytes()
    for secret in secrets:
        if secret.encode("utf-8") in app:
            sys.exit(f"FAILED ({name}): a value from include/secrets.h is inside the binary")
    print(f"OK ({name}): no WiFi credentials from include/secrets.h in the binary")

    fw_dir = out / "firmware"
    fw_dir.mkdir(parents=True, exist_ok=True)
    (out / "ota").mkdir(parents=True, exist_ok=True)
    for old in fw_dir.glob(f"{prefix}-*.bin"):
        old.unlink()
    app_name = f"{prefix}-{version}.bin"
    (fw_dir / app_name).write_bytes(app)
    # Bootloader and partition table are shared by the boards (same chip and
    # partitions): they must come out identical, or the installer would mix them.
    for part in SHARED_PARTS:
        data = (build_dir / part).read_bytes()
        target = fw_dir / part
        if target.exists() and target.read_bytes() != data and name != BOARDS[0][0]:
            sys.exit(f"{name}: {part} differs from the one already in {fw_dir}")
        target.write_bytes(data)
    shutil.copy(BOOT_APP0, fw_dir / "boot_app0.bin")
    (out / ".nojekyll").write_text("", encoding="utf-8")  # serve files as they are

    latest = {"version": version, "url": f"{base_url}/firmware/{app_name}",
              "size": len(app), "md5": hashlib.md5(app).hexdigest()}
    (out / "ota" / ota_name).write_text(json.dumps(latest, indent=2) + "\n", encoding="utf-8")

    parts = [{"path": f"firmware/{n}", "offset": OFFSETS[n]}
             for n in ("bootloader.bin", "partitions.bin", "boot_app0.bin")]
    parts.append({"path": f"firmware/{app_name}", "offset": OFFSETS["app"]})
    web = {"name": title, "version": version,
           "new_install_prompt_erase": False,  # keep WiFi, cities and calibration
           "builds": [{"chipFamily": "ESP32", "parts": parts}]}
    (out / web_name).write_text(json.dumps(web, indent=2) + "\n", encoding="utf-8")
    print(f"release {version} {name}: {len(app)} bytes, md5 {latest['md5']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-build", action="store_true", help="skip the PlatformIO builds")
    parser.add_argument("--board", choices=[b[0] for b in BOARDS], action="append",
                        help="release only this board (repeatable; default: all)")
    parser.add_argument("--out", type=Path, default=DOCS, help="output folder (default docs/)")
    args = parser.parse_args()

    version = config_string("kFirmwareVersion")
    base_url = define_string("include/config.h", "CYD_PAGES_URL")
    secrets = secrets_strings()
    out = args.out if args.out.is_absolute() else ROOT / args.out
    for board in BOARDS:
        if args.board is None or board[0] in args.board:
            release_board(board, version, base_url, out, args.no_build, secrets)
    if out == DOCS:
        print("next: git add docs && git commit -m \"Release v" + version + "\" && git push")


if __name__ == "__main__":
    main()
