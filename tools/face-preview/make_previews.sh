#!/usr/bin/env bash
# Build the face previewer and render every expression to PNG (needs g++ and Python Pillow).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/out}"
mkdir -p "$out"
g++ -O2 -std=c++17 -I"$here/../../firmware/src" "$here/preview.cpp" "$here/../../firmware/src/face.cpp" -o "$out/preview"
"$out/preview" "$out"
python3 - "$out" <<'PY'
import sys, glob, os
from PIL import Image
out = sys.argv[1]
names = []
for p in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    Image.open(p).save(p[:-4] + ".png"); os.remove(p); names.append(p[:-4] + ".png")
# contact sheet, 4 per row, 2x scale
order = ["content","happy","cheer","talking","listening","thinking","surprised","dizzy","sleepy","asleep","love","look-left"]
imgs = [Image.open(os.path.join(out, n + ".png")) for n in order if os.path.exists(os.path.join(out, n + ".png"))]
cols, pad = 4, 16
W = cols * (240 + pad) + pad; H = ((len(imgs) + cols - 1) // cols) * (240 + pad) + pad
sheet = Image.new("RGB", (W, H), (0xF5, 0xE6, 0xC8))
for i, im in enumerate(imgs):
    sheet.paste(im, (pad + (i % cols) * (240 + pad), pad + (i // cols) * (240 + pad)))
sheet.save(os.path.join(out, "all-expressions.png"))
print("wrote", len(imgs), "faces to", out)
PY
