#!/bin/bash
# i686 MiniOS Music window.
set -euo pipefail
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
SRC="$POC/scripts/minios-mp3.c"
OUT="$POC/scripts/minios-mp3"
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
ls -l "$OUT"
echo COMPILE_MP3_OK
