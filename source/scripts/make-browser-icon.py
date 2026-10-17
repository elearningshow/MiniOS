#!/usr/bin/env python3
"""Tiny 32x32 black / cyan browser icon (SWAP-AP-01 v2.2)."""
import struct
import zlib
from pathlib import Path

W = H = 32
BLACK = (0, 0, 0, 255)
CYAN = (0, 229, 255, 255)


def px(img, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def hline(img, x0, x1, y, c):
    for x in range(x0, x1 + 1):
        px(img, x, y, c)


def vline(img, x, y0, y1, c):
    for y in range(y0, y1 + 1):
        px(img, x, y, c)


def rect(img, x0, y0, x1, y1, c, fill=True):
    if fill:
        for y in range(y0, y1 + 1):
            hline(img, x0, x1, y, c)
    else:
        hline(img, x0, x1, y0, c)
        hline(img, x0, x1, y1, c)
        vline(img, x0, y0, y1, c)
        vline(img, x1, y0, y1, c)


img = [[BLACK for _ in range(W)] for _ in range(H)]
for i in range(2):
    hline(img, 0, 31, i, CYAN)
    hline(img, 0, 31, 31 - i, CYAN)
    vline(img, i, 0, 31, CYAN)
    vline(img, 31 - i, 0, 31, CYAN)
# window chrome
rect(img, 6, 7, 25, 24, CYAN, fill=False)
hline(img, 6, 25, 11, CYAN)
# globe-ish
rect(img, 11, 14, 20, 21, CYAN, fill=False)
vline(img, 15, 14, 21, CYAN)
hline(img, 11, 20, 17, CYAN)

raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in img)


def chunk(tag, data):
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


png = (
    b"\x89PNG\r\n\x1a\n"
    + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
    + chunk(b"IDAT", zlib.compress(raw, 9))
    + chunk(b"IEND", b"")
)
out = Path(__file__).resolve().parents[3] / "branding" / "icons" / "apps" / "browser.png"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(png)
print(f"wrote {out} ({out.stat().st_size} bytes)")
