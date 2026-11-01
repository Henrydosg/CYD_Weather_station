"""Publish a message ("Lời nhắn") for the devices: writes docs/message.json.

Devices read it from GitHub Pages every 30 minutes (config::kMessageUrl) and
show it full screen until "OK" is tapped. A new message gets a new id, so it
shows again even on a device that closed the previous one.

Usage (from the project root):
    python tools/message.py "Chúc mừng sinh nhật!" --from Henry
    python tools/message.py "Chúc mừng năm mới!" --show-from 2027-02-06 \
        --show-until 2027-02-09
    python tools/message.py --clear          no message
Then: git add docs/message.json && git commit -m "Message" && git push
(GitHub Pages may serve the old file for up to 10 minutes.)

Dates are local dates in Vietnam (UTC+7): shown from 00:00 of --show-from
until 00:00 of the day after --show-until. Only characters of the device
fonts are allowed (ASCII, Vietnamese, °): no emoji.
"""

import argparse
import json
import sys
import time
import unicodedata
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_fonts import TEXT_CHARS  # noqa: E402

OUT = ROOT / "docs" / "message.json"
LOCAL = timezone(timedelta(hours=7))
MAX_TEXT_BYTES = 240  # config::kMaxMessageLen
MAX_FROM_BYTES = 31   # config::kMaxMessageFromLen


def local_midnight(text, days_after=0):
    day = datetime.strptime(text, "%Y-%m-%d").replace(tzinfo=LOCAL)
    return int((day + timedelta(days=days_after)).timestamp())


def check_text(label, text, max_bytes):
    text = unicodedata.normalize("NFC", text.strip())
    missing = sorted({c for c in text if c not in TEXT_CHARS and c != "\n"})
    if missing:
        sys.exit(f"{label}: the device fonts have no glyph for: {' '.join(missing)}")
    if len(text.encode("utf-8")) > max_bytes:
        sys.exit(f"{label}: too long ({len(text.encode('utf-8'))} bytes, max {max_bytes})")
    return text


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("text", nargs="?", help="message text (use \\n for a new line)")
    parser.add_argument("--from", dest="sender", default="", help="sender name shown in the title")
    parser.add_argument("--show-from", help="first day, YYYY-MM-DD (default: now)")
    parser.add_argument("--show-until", help="last day, YYYY-MM-DD (default: no end)")
    parser.add_argument("--clear", action="store_true", help="publish no message")
    args = parser.parse_args()

    if args.clear:
        msg = {"id": 0}
    else:
        if not args.text:
            parser.error("text is required (or --clear)")
        msg = {
            "id": int(time.time()),
            "from": check_text("--from", args.sender, MAX_FROM_BYTES),
            "text": check_text("text", args.text.replace("\\n", "\n"), MAX_TEXT_BYTES),
            "show_from": local_midnight(args.show_from) if args.show_from else 0,
            "show_until": local_midnight(args.show_until, 1) if args.show_until else 0,
        }
        if msg["show_until"] and msg["show_until"] <= max(msg["show_from"], int(time.time())):
            sys.exit("--show-until is already over")
    OUT.write_text(json.dumps(msg, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}: " + ("no message" if args.clear else f"id {msg['id']}"))
    print('next: git add docs/message.json && git commit -m "Message" && git push')


if __name__ == "__main__":
    main()
