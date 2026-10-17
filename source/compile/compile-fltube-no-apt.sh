#!/bin/bash
# Compile official FLTube 2.1.2 as i686 ELF. Skips apt-get (already have g++ -m32).
set -euo pipefail
SRC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/fltube-src
OUT=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/tcz-fltube
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
LIB=/tmp/fltube-tclibs
if [[ ! -f "$LIB/fltk-dev/usr/local/lib/libfltk.a" ]]; then
  rm -rf "$LIB"
  mkdir -p "$LIB"
  for t in fltk-1.3 curl libX11 libXext libXft libXrender fontconfig freetype libpng libjpeg-turbo openssl libXcursor libxcb libXau libXdmcp libzstd libXfixes; do
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
  [[ -f "$POC/tcz-fltube/curl-dev.tcz" ]] && unsquashfs -f -d "$LIB/curl-dev" "$POC/tcz-fltube/curl-dev.tcz" >/dev/null
fi
[[ -f "$LIB/fltk-dev/usr/local/lib/libfltk.a" ]] || { echo "missing libfltk.a"; exit 1; }
cd "$SRC"
mkdir -p build
INC="-Iinclude"
[[ -d $LIB/fltk-dev/usr/local/include ]] && INC="$INC -I$LIB/fltk-dev/usr/local/include"
[[ -d $LIB/curl-dev/usr/local/include ]] && INC="$INC -I$LIB/curl-dev/usr/local/include"
INC="$INC -I/usr/include -I/usr/include/x86_64-linux-gnu"
CXX=g++
for src in fltube_utils gnugettext_utils FLTube_View FLTube configuration_manager userdata_manager ytdlp_helper cache custom_widgets; do
  echo "CC $src"
  $CXX -m32 -std=c++17 -O2 -fexceptions -fno-rtti $INC -DVERSION_STRING='"2.1.2"' \
    -c src/${src}.cxx -o build/${src}.o
done
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
$CXX -m32 -o build/fltube build/*.o \
  "$FLTKA/libfltk_images.a" "$FLTKA/libfltk.a" \
  $LPATHS $RPATH \
  -lcurl -lXcursor -lXfixes -lXft -lXrender -lXext -lX11 \
  -lfontconfig -lfreetype -lpng -ljpeg \
  -lxcb -lXau -lXdmcp -lzstd -lz -lpthread -lstdc++
file build/fltube
ls -lh build/fltube
mkdir -p "$OUT"
cp -f build/fltube "$OUT/fltube"
echo COMPILE_FLTUBE_OK
