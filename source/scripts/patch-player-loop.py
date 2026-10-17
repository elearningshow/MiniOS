#!/usr/bin/env python3
"""Patch MiniOS player-bar ELF: mplayer slave 'loop 1' -> 'loop -1'.

Does not recompile. 106 YouTube player-bar must stay the 106 binary;
only the native-loop slave command changes. Same-length in-place edit.
"""
import sys
from pathlib import Path

OLD = b"loop 1\n\x00osd 1\n"
NEW = b"loop -1\nosd 1\n"

if len(OLD) != len(NEW):
    sys.exit("length mismatch")

if len(sys.argv) != 3:
    sys.exit("usage: patch-player-loop.py IN OUT")

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
data = src.read_bytes()
n = data.count(OLD)
if n != 1:
    sys.exit("expected one %r in %s, found %d" % (OLD, src, n))
dst.write_bytes(data.replace(OLD, NEW, 1))
print("PATCH_LOOP_OK %s -> %s" % (src, dst))
