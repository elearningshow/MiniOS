#!/bin/bash
# Compile minios-yt-focus (X11 helper). Same flags as compile-cad.sh.
set -euo pipefail
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
SRC="$POC/scripts/minios-yt-focus.c"
OUT="$POC/scripts/minios-yt-focus"
LIB=/tmp/minios-x11libs
if [[ ! -f "$LIB/libX11/usr/local/lib/libX11.so.6" ]]; then
  bash "$POC/prep-x11-libs.sh"
fi
gcc -m32 -O2 -Wall -o "$OUT" "$SRC" \
  -L"$LIB/libX11/usr/local/lib" \
  -L"$LIB/libxcb/usr/local/lib" \
  -L"$LIB/libXau/usr/local/lib" \
  -L"$LIB/libXdmcp/usr/local/lib" \
  -Wl,-rpath-link,"$LIB/libX11/usr/local/lib:$LIB/libxcb/usr/local/lib:$LIB/libXau/usr/local/lib:$LIB/libXdmcp/usr/local/lib" \
  -lX11 -lxcb -lXau -lXdmcp
file "$OUT"
echo COMPILE_YT_FOCUS_OK
