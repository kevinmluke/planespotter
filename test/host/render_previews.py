#!/usr/bin/env python3
"""Converts build/previews/*.pbm into docs/previews/*.png (2x, with a bezel)."""
import glob
import os

from PIL import Image, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "docs", "previews")
os.makedirs(OUT, exist_ok=True)

for path in sorted(glob.glob("build/previews/*.pbm")):
    img = Image.open(path).convert("L")
    # E-paper look: warm grey "white", soft black
    img = img.point(lambda v: 232 if v > 128 else 28)
    img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    framed = ImageOps.expand(img, border=24, fill=245)
    name = os.path.splitext(os.path.basename(path))[0]
    framed.save(os.path.join(OUT, name + ".png"))
    print("wrote", name + ".png")
