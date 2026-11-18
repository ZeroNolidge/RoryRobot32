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


# Thinking sounds, made in Rory's own voice by your server (needs --server/--token,
# or a config.txt already on the card).
def _made_lines() -> dict:
    """The lines Rory makes in her own voice: read from the firmware's list
    (firmware/src/made_clips.h), so there's only one list to keep up to date."""
    path = os.path.join(HERE, "..", "firmware", "src", "made_clips.h")
    lines = {}
    try:
        for moment, line in re.findall(r'\{"([^"]+)",\s*"((?:[^"\\]|\\.)*)"\}', open(path, encoding="utf-8").read()):
            lines.setdefault(moment, []).append(line.replace('\\"', '"'))
    except OSError:
        pass
    return lines


MADE_LINES = _made_lines()
# (Rory also makes any of these that are missing by herself over Wi-Fi.)


MEMORY_HEADER = """# What Rory remembers about her friend. She adds a line when she learns something new.
# Grown-ups: change or delete any line. One fun fact per line, e.g.  His name is Sam.
# (No addresses, school, last names or other private details.)
"""


def write_memory(card: str, name: str) -> None:
    path = os.path.join(card, "rory", "memory.txt")
    lines = []
    if os.path.exists(path):
        lines = open(path, encoding="utf-8").read().splitlines()
    elif not name:
        return  # Rory starts one herself when she learns something
    if name:
        fact = f"His name is {name}."
        if any(l.strip().lower() == fact.lower() for l in lines):
            return
        lines = [l for l in lines if not l.strip().lower().startswith("his name is")]
        lines.append(fact)
    body = "\n".join(lines).strip()
    if not body.startswith("#"):
        body = MEMORY_HEADER + body
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(body.rstrip() + "\n")
    print(f"Memory: {path} - Rory knows his name is {name}")


def read_config(card: str) -> dict:
    path = os.path.join(card, "rory", "config.txt")
    out = {}
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            if "=" in line and not line.strip().startswith("#"):
                k, v = line.split("=", 1)
                out[k.strip().lower()] = v.strip()
    return out


def make_thinking_clips(card: str, server: str, token: str) -> None:
    made = 0
    for moment, lines in MADE_LINES.items():
        made += make_clips(card, server, token, moment, lines)
    if made:
        print(f"Made {made} clips in Rory's voice")


def make_clips(card: str, server: str, token: str, moment: str, lines: list) -> int:
    folder = os.path.join(card, "rory", moment)
    os.makedirs(folder, exist_ok=True)
    made = 0
    for line in lines:
        dest = os.path.join(folder, slug(line) + ".mp3")
        if os.path.exists(dest):
            continue
        req = urllib.request.Request(server.rstrip("/") + "/say", data=line.encode("utf-8"), method="POST",
                                     headers={"Authorization": "Bearer " + token, "Content-Type": "text/plain",
                                              # Cloudflare blocks Python's default "Python-urllib" name (403)
                                              "User-Agent": "RorySDCard/1.0"})
        try:
            with urllib.request.urlopen(req, timeout=60) as r, open(dest, "wb") as f:
                f.write(r.read())
            made += 1
            print(f"  {moment:10s} {line}")
        except Exception as e:  # keep going; Rory just thinks quietly without these
            print(f"  (couldn't make \"{line}\": {e})")
    return made


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("card", help="where the SD card is mounted (or any folder to copy later)")
    ap.add_argument("--app", help="local clone of the SpeechApp repo (otherwise download from GitHub)")
    ap.add_argument("--wifi-name", help="your Wi-Fi network name (2.4 GHz)")
    ap.add_argument("--wifi-password", help="your Wi-Fi password")
    ap.add_argument("--server", help="Rory's server address, e.g. https://rory-server.NAME.workers.dev")
    ap.add_argument("--token", help="the RORY_DEVICE_TOKEN you set on the server")
    ap.add_argument("--name", help="his first name, so Rory knows it from the start (saved in rory/memory.txt)")
    args = ap.parse_args()
    write_config(args)
    write_memory(args.card, (args.name or "").strip())

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

    cfg = read_config(args.card)
    server = (args.server or cfg.get("server", "")).strip()
    token = (args.token or cfg.get("token", "")).strip()
    if server.startswith("https://") and token:
        make_thinking_clips(args.card, server, token)
    else:
        print("(Add --server and --token to also make Rory's \"Hmm, let me think!\" clips.)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
