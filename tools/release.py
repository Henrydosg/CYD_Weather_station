"""Build and publish a firmware release to docs/ (GitHub Pages).

Steps:
1. Reads config::kFirmwareVersion and config::kOtaManifestUrl from include/config.h.
2. Builds the PlatformIO env "cyd-release" (no include/secrets.h inside).
3. Checks that the WiFi SSID / password of include/secrets.h do not appear in
   the binary (prints only OK / FAILED, never the values).
4. Writes, under docs/:
     firmware/cyd-weather-<version>.bin     app image (OTA and web installer)
     firmware/bootloader.bin, partitions.bin, boot_app0.bin   (web installer)
     ota/latest.json                        {"version", "url", "size", "md5"}
     manifest.json                          ESP Web Tools manifest (docs/index.html)
   Older cyd-weather-*.bin files are removed (git history keeps them).

Then commit and push docs/; devices see the update within a day, or at once
with Settings -> Firmware -> Check. GitHub Pages must serve the main branch
/docs folder (repository Settings -> Pages).

Usage (from the project root):
    python tools/release.py            build and write docs/
    python tools/release.py --no-build reuse .pio/build/cyd-release
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
ENV = "cyd-release"
BUILD = ROOT / ".pio" / "build" / ENV
DOCS = ROOT / "docs"
PIO = Path(os.environ.get("USERPROFILE", str(Path.home()))) / ".platformio"
BOOT_APP0 = PIO / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"
MANIFEST_SUFFIX = "/ota/latest.json"
# Flash offsets of the parts (ESP32, min_spiffs.csv partition table)
OFFSETS = {"bootloader.bin": 0x1000, "partitions.bin": 0x8000, "boot_app0.bin": 0xE000,
           "app": 0x10000}


def config_string(name):
    text = (ROOT / "include" / "config.h").read_text(encoding="utf-8")
    m = re.search(name + r'\[\]\s*=\s*\s*"([^"]+)"', text)
    if not m:
        sys.exit(f"{name} not found in include/config.h")
    return m.group(1)


def secrets_strings():
    path = ROOT / "include" / "secrets.h"
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    return [v for v in re.findall(r'k\w+\[\]\s*=\s*"([^"]*)"', text) if len(v) >= 4]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-build", action="store_true", help="skip the PlatformIO build")
    args = parser.parse_args()

    version = config_string("kFirmwareVersion")
    manifest_url = config_string("kOtaManifestUrl")
    if not manifest_url.endswith(MANIFEST_SUFFIX):
        sys.exit(f"kOtaManifestUrl must end with {MANIFEST_SUFFIX}")
    base_url = manifest_url[: -len(MANIFEST_SUFFIX)]

    if not args.no_build:
        pio = PIO / "penv" / "Scripts" / "pio.exe"
        cmd = [str(pio) if pio.exists() else "pio", "run", "-e", ENV]
        if subprocess.run(cmd, cwd=ROOT).returncode != 0:
            sys.exit("build failed")

    app = (BUILD / "firmware.bin").read_bytes()
    for secret in secrets_strings():
        if secret.encode("utf-8") in app:
            sys.exit("FAILED: a value from include/secrets.h is inside the release binary")
    print("OK: no WiFi credentials from include/secrets.h in the binary")

    fw_dir = DOCS / "firmware"
    fw_dir.mkdir(parents=True, exist_ok=True)
    (DOCS / "ota").mkdir(parents=True, exist_ok=True)
    for old in fw_dir.glob("cyd-weather-*.bin"):
        old.unlink()
    app_name = f"cyd-weather-{version}.bin"
    (fw_dir / app_name).write_bytes(app)
    shutil.copy(BUILD / "bootloader.bin", fw_dir / "bootloader.bin")
    shutil.copy(BUILD / "partitions.bin", fw_dir / "partitions.bin")
    shutil.copy(BOOT_APP0, fw_dir / "boot_app0.bin")
    (DOCS / ".nojekyll").write_text("", encoding="utf-8")  # serve files as they are

    latest = {"version": version, "url": f"{base_url}/firmware/{app_name}",
              "size": len(app), "md5": hashlib.md5(app).hexdigest()}
    (DOCS / "ota" / "latest.json").write_text(json.dumps(latest, indent=2) + "\n",
                                              encoding="utf-8")

    parts = [{"path": f"firmware/{n}", "offset": OFFSETS[n]}
             for n in ("bootloader.bin", "partitions.bin", "boot_app0.bin")]
    parts.append({"path": f"firmware/{app_name}", "offset": OFFSETS["app"]})
    web = {"name": "CYD Weather Station", "version": version,
           "new_install_prompt_erase": False,  # keep WiFi, cities and calibration
           "builds": [{"chipFamily": "ESP32", "parts": parts}]}
    (DOCS / "manifest.json").write_text(json.dumps(web, indent=2) + "\n", encoding="utf-8")

    print(f"release {version}: {len(app)} bytes, md5 {latest['md5']}")
    print("next: git add docs && git commit -m \"Release v" + version + "\" && git push")


if __name__ == "__main__":
    main()
