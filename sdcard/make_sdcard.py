#!/usr/bin/env python3
"""Fill a microSD card with Rory's voice clips from the Talk Trail app.

Usage:
    python3 make_sdcard.py /Volumes/RORY                 # download clips from GitHub
    python3 make_sdcard.py E:\\ --app ../../SpeechApp-test  # copy from a local clone

    # also set up talking to Rory's AI server:
    python3 make_sdcard.py /Volumes/RORY --wifi-name "Home" --wifi-password "..." \\
        --server https://rory-server.NAME.workers.dev --token "<RORY_DEVICE_TOKEN>"

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


CONFIG_TEMPLATE = """# Rory's settings. Lines starting with # are ignored.
# Fill these in to let Rory answer questions with her AI server
# (see server/README.md). Without them she still plays, just offline.
wifi_name = {wifi_name}
wifi_password = {wifi_password}
server = {server}
token = {token}
"""


def write_config(args) -> None:
    path = os.path.join(args.card, "rory", "config.txt")
    given = any([args.wifi_name, args.wifi_password, args.server, args.token])
    if os.path.exists(path) and not given:
        return  # keep the settings that are already there
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(CONFIG_TEMPLATE.format(
            wifi_name=args.wifi_name or "", wifi_password=args.wifi_password or "",
            server=(args.server or "").rstrip("/"), token=args.token or ""))
    print(f"Settings written to {path}" + ("" if given else " (empty - fill it in to connect Rory)"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("card", help="where the SD card is mounted (or any folder to copy later)")
    ap.add_argument("--app", help="local clone of the SpeechApp repo (otherwise download from GitHub)")
    ap.add_argument("--wifi-name", help="your Wi-Fi network name (2.4 GHz)")
    ap.add_argument("--wifi-password", help="your Wi-Fi password")
    ap.add_argument("--server", help="Rory's server address, e.g. https://rory-server.NAME.workers.dev")
    ap.add_argument("--token", help="the RORY_DEVICE_TOKEN you set on the server")
    args = ap.parse_args()
    write_config(args)

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
