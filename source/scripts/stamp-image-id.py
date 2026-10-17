#!/usr/bin/env python3
"""Stamp IMAGE-ID (exact .img basename) on the lower-right of the wallpaper."""
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.stderr.write("Pillow required\n")
    sys.exit(1)

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
text = sys.argv[3]
im = Image.open(src).convert("RGB")
draw = ImageDraw.Draw(im)
font = None
for fp in (
    "C:/Windows/Fonts/tahoma.ttf",
    "C:/Windows/Fonts/TAHOMA.TTF",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
):
    p = Path(fp)
    if p.is_file():
        font = ImageFont.truetype(str(p), 22)
        break
if font is None:
    font = ImageFont.load_default()
bbox = draw.textbbox((0, 0), text, font=font)
tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
x = im.width - tw - 16
y = im.height - th - 12
# dark box so old leftover stamps do not show through
draw.rectangle((x - 8, y - 4, im.width - 4, im.height - 4), fill=(8, 16, 32))
draw.text((x, y), text, font=font, fill=(160, 168, 176))
im.save(dst, "JPEG", quality=90)
print("STAMPED", dst, text)
