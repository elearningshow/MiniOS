#!/usr/bin/env python3
"""48x48 Games dock icon. Black field, cyan border, simple pad. No third-party libs."""
import struct
import zlib
from pathlib import Path

W = H = 48
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


def fill(img, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        hline(img, x0, x1, y, c)


def disc(img, cx, cy, r, c):
    r2 = r * r
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r2:
                px(img, x, y, c)


img = [[BLACK for _ in range(W)] for _ in range(H)]
for i in range(2):
    hline(img, 0, W - 1, i, CYAN)
    hline(img, 0, W - 1, H - 1 - i, CYAN)
    vline(img, i, 0, H - 1, CYAN)
    vline(img, W - 1 - i, 0, H - 1, CYAN)
fill(img, 8, 16, 39, 32, CYAN)
disc(img, 14, 24, 5, BLACK)
disc(img, 34, 24, 5, BLACK)
hline(img, 22, 26, 22, BLACK)
hline(img, 22, 26, 26, BLACK)
vline(img, 24, 20, 28, BLACK)

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
out = Path(__file__).resolve().parent / "games.png"
out.write_bytes(png)
print("wrote %s (%d bytes)" % (out, out.stat().st_size))
