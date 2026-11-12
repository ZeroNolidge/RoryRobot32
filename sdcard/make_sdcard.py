#!/usr/bin/env python3
"""Fill a microSD card with Rory's voice clips from the Talk Trail app.

Usage:
    python3 make_sdcard.py /Volumes/RORY                 # download clips from GitHub
    python3 make_sdcard.py E:\\ --app ../../SpeechApp-test  # copy from a local clone

Creates  <card>/rory/<moment>/<words>.mp3  for every moment listed in
rory_clips.json. Add your own MP3s to any of those folders and Rory will use
them too (e.g. record "Hi Rory's name!" into rory/hello/).
"""
import argparse
import json
import os
import re
import shutil
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = "https://raw.githubusercontent.com/ZeroNolidge/SpeechApp-test/main/audio/"


def slug(text: str) -> str:
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return (s or "clip")[:40]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("card", help="where the SD card is mounted (or any folder to copy later)")
    ap.add_argument("--app", help="local clone of the SpeechApp repo (otherwise download from GitHub)")
    args = ap.parse_args()

    spec = json.load(open(os.path.join(HERE, "rory_clips.json")))
    total = 0
    for moment, clips in spec["moments"].items():
        folder = os.path.join(args.card, "rory", moment)
        os.makedirs(folder, exist_ok=True)
        for clip in clips:
            dest = os.path.join(folder, slug(clip["says"]) + ".mp3")
            if os.path.exists(dest):
                continue
            if args.app:
                shutil.copyfile(os.path.join(args.app, "audio", clip["file"]), dest)
            else:
                with urllib.request.urlopen(RAW + clip["file"], timeout=30) as r, open(dest, "wb") as f:
                    f.write(r.read())
            total += 1
            print(f"  {moment:10s} {clip['says']}")
    print(f"Done: {total} new clips in {os.path.join(args.card, 'rory')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
