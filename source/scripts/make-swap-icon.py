#!/usr/bin/env python3
"""Tiny 32x32 black / cyan swap icon (SWAP-AP-01). No third-party libs."""
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
# 2px cyan frame
for i in range(2):
    hline(img, 0, 31, i, CYAN)
    hline(img, 0, 31, 31 - i, CYAN)
    vline(img, i, 0, 31, CYAN)
    vline(img, 31 - i, 0, 31, CYAN)
# two memory slabs
rect(img, 7, 8, 24, 13, CYAN)
rect(img, 7, 18, 24, 23, CYAN)
# double arrow between slabs
hline(img, 15, 16, 14, CYAN)
hline(img, 15, 16, 17, CYAN)
px(img, 14, 15, CYAN)
px(img, 17, 15, CYAN)
px(img, 14, 16, CYAN)
px(img, 17, 16, CYAN)

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

# XPM for IceWM
xpm_rows = []
for y in range(H):
    s = ""
    for x in range(W):
        s += "." if img[y][x] == CYAN else " "
    xpm_rows.append(s)
xpm = (
    "/* XPM */\n"
    "static char * create_swap_xpm[] = {\n"
    f'"{W} {H} 2 1",\n'
    '"  c #000000",\n'
    '". c #00E5FF",\n'
    + ",\n".join(f'"{r}"' for r in xpm_rows)
    + "\n};\n"
)

out_dir = Path(__file__).resolve().parents[3] / "branding" / "icons" / "apps"
out_dir.mkdir(parents=True, exist_ok=True)
png_path = out_dir / "create-swap.png"
xpm_path = out_dir / "create-swap.xpm"
png_path.write_bytes(png)
xpm_path.write_text(xpm, encoding="ascii")
print(f"wrote {png_path} ({png_path.stat().st_size} bytes)")
print(f"wrote {xpm_path} ({xpm_path.stat().st_size} bytes)")
