#!/usr/bin/env python3
"""Cut studio disc photos to 48x48 circular PNGs (transparent corners)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

SRC = Path("/mnt/f/temp")
OUT = Path("/mnt/f/AI/xAI-projects/MiniOS/artifacts/MiniOS/icons")
SIZE = 48

JOBS = (
    "YGSKE.jpg",
    "0XXRf.jpg",
    "ZV1MI.jpg",
    "Xhvm6.jpg",
    "uJZ9u.jpg",
    "Mcnt2.jpg",
)


def disc_radius(im):
    g = im.convert("L")
    w, h = g.size
    cx, cy = w // 2, h // 2
    bg = (
        g.getpixel((2, 2))
        + g.getpixel((w - 3, 2))
        + g.getpixel((2, h - 3))
        + g.getpixel((w - 3, h - 3))
    ) / 4.0
    max_r = min(cx, cy) - 2
    r = max_r
    while r > 8:
        samples = []
        for k in range(24):
            import math
            a = k * math.pi / 12.0
            x = int(cx + r * math.cos(a))
            y = int(cy + r * math.sin(a))
            if 0 <= x < w and 0 <= y < h:
                samples.append(g.getpixel((x, y)))
        if not samples:
            break
        mean = sum(samples) / len(samples)
        # Inside the glossy disc is darker than the gray backdrop.
        if abs(mean - bg) > 18:
            return r
        r -= 1
    return int(max_r * 0.42)


def cut_disc(src, dest):
    im = Image.open(src).convert("RGB")
    w, h = im.size
    cx, cy = w // 2, h // 2
    r = disc_radius(im)
    # Include the rim; keep a 1px transparent ring after scale (Exit silhouette).
    pad = max(4, r // 18)
    box = (cx - r - pad, cy - r - pad, cx + r + pad, cy + r + pad)
    crop = im.crop(box).resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    mask = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(mask)
    # Inscribed circle, 1px inset so corners stay fully transparent.
    d.ellipse((1, 1, SIZE - 2, SIZE - 2), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(0.4))
    out = crop.convert("RGBA")
    out.putalpha(mask)
    dest.parent.mkdir(parents=True, exist_ok=True)
    out.save(dest, "PNG")
    print(f"wrote {dest} {out.size} r={r}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name in JOBS:
        src = SRC / name
        dest = OUT / (Path(name).stem + ".png")
        if not src.is_file():
            raise SystemExit(f"missing {src}")
        cut_disc(src, dest)


if __name__ == "__main__":
    main()
