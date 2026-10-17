#!/usr/bin/env python3
"""32x32 MP3 Player note icon. No third-party libs."""
import struct
import zlib
from pathlib import Path

W = H = 32
BLACK = (0, 0, 0, 255)
GOLD = (200, 160, 0, 255)


def px(img, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def hline(img, x0, x1, y, c):
    for x in range(x0, x1 + 1):
        px(img, x, y, c)


def vline(img, x, y0, y1, c):
    for y in range(y0, y1 + 1):
        px(img, x, y, c)


def fill(img, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        hline(img, x0, x1, y, c)


img = [[BLACK for _ in range(W)] for _ in range(H)]
for i in range(2):
    hline(img, 0, 31, i, GOLD)
    hline(img, 0, 31, 31 - i, GOLD)
    vline(img, i, 0, 31, GOLD)
    vline(img, 31 - i, 0, 31, GOLD)
# note head
fill(img, 8, 20, 15, 26, GOLD)
# stem
vline(img, 15, 8, 26, GOLD)
vline(img, 16, 8, 26, GOLD)
# flag
fill(img, 16, 8, 24, 12, GOLD)
hline(img, 17, 23, 13, GOLD)

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
out = Path(__file__).resolve().parent.parent / "branding" / "mp3-player.png"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(png)
print("wrote %s (%s bytes)" % (out, out.stat().st_size))
