#!/bin/bash
# i686 MiniOS player bar — FLTK 1.3 (static libfltk.a, no new package).
set -euo pipefail
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
SRC="$POC/scripts/minios-player-bar.cxx"
OUT="$POC/scripts/minios-player-bar"
LIB=/tmp/fltube-tclibs
if [[ ! -f "$LIB/fltk-dev/usr/local/lib/libfltk.a" ]]; then
  rm -rf "$LIB"
  mkdir -p "$LIB"
  for t in fltk-1.3 libX11 libXext libXft libXrender fontconfig freetype libpng libjpeg-turbo libXcursor libxcb libXau libXdmcp libzstd libXfixes; do
    for dir in "$POC/tcz" "$POC/tcz-fltube" \
               /mnt/f/AI/xAI-projects/MiniOS/artifacts/MiniOS-live/image/cde/optional; do
      if [[ -f "$dir/${t}.tcz" ]]; then
        unsquashfs -f -d "$LIB/$t" "$dir/${t}.tcz" >/dev/null
        echo "unsquash $t"
        break
      fi
    done
  done
  [[ -f "$POC/tcz-fltube/fltk-1.3-dev.tcz" ]] && unsquashfs -f -d "$LIB/fltk-dev" "$POC/tcz-fltube/fltk-1.3-dev.tcz" >/dev/null
fi
[[ -f "$LIB/fltk-dev/usr/local/lib/libfltk.a" ]] || { echo "missing libfltk.a"; exit 1; }

INC="-I$LIB/fltk-dev/usr/local/include -I/usr/include -I/usr/include/x86_64-linux-gnu"
LPATHS=""
RPATH=""
for d in "$LIB"/*/usr/local/lib; do
  [[ -d "$d" ]] || continue
  LPATHS="$LPATHS -L$d"
  RPATH="$RPATH -Wl,-rpath-link,$d"
done
for z in /usr/lib/i386-linux-gnu /lib/i386-linux-gnu /usr/lib32; do
  [[ -d "$z" ]] && LPATHS="$LPATHS -L$z"
done
FLTKA="$LIB/fltk-dev/usr/local/lib"
g++ -m32 -std=c++17 -O2 -Wall -fno-rtti -fexceptions $INC -o "$OUT" "$SRC" \
  "$FLTKA/libfltk.a" \
  $LPATHS $RPATH \
  -lXcursor -lXfixes -lXft -lXrender -lXext -lX11 \
  -lfontconfig -lfreetype -lpng \
  -lxcb -lXau -lXdmcp -lzstd -lz -lpthread -lstdc++
strip "$OUT" || true
file "$OUT"
ls -l "$OUT"
echo COMPILE_PLAYER_BAR_OK
