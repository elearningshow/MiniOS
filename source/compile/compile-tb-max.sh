#!/bin/bash
# Compile the Text Browser maximize helper. Same flags as compile-yt-focus.sh.
set -euo pipefail
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
SRC="$POC/scripts/minios-tb-max.c"
OUT="$POC/scripts/minios-tb-max"
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
echo COMPILE_TB_MAX_OK
