#!/usr/bin/env python3
"""32x32 cyan Refresh RAM icon. No third-party libs."""
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


def rect(img, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        hline(img, x0, x1, y, c)


img = [[BLACK for _ in range(W)] for _ in range(H)]
for i in range(2):
    hline(img, 0, 31, i, CYAN)
    hline(img, 0, 31, 31 - i, CYAN)
    vline(img, i, 0, 31, CYAN)
    vline(img, 31 - i, 0, 31, CYAN)
# memory slab
rect(img, 8, 12, 23, 19, CYAN)
hline(img, 10, 21, 14, BLACK)
hline(img, 10, 21, 17, BLACK)
# circular refresh arrows
for x, y in (
    (10, 6), (11, 6), (12, 6), (13, 6), (14, 6),
    (9, 7), (15, 7),
    (8, 8), (8, 9),
    (21, 22), (22, 22), (23, 22),
    (16, 25), (17, 25), (18, 25), (19, 25), (20, 25),
    (21, 24), (15, 24),
    (22, 23), (23, 21),
):
    px(img, x, y, CYAN)
# arrow heads
px(img, 14, 5, CYAN)
px(img, 15, 6, CYAN)
px(img, 14, 7, CYAN)
px(img, 16, 24, CYAN)
px(img, 16, 26, CYAN)
px(img, 17, 25, CYAN)

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

out_dir = Path(__file__).resolve().parent.parent / "branding"
out_dir.mkdir(parents=True, exist_ok=True)
png_path = out_dir / "refresh-ram.png"
png_path.write_bytes(png)
print(f"wrote {png_path} ({png_path.stat().st_size} bytes)")
