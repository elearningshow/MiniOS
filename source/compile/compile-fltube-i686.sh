#!/bin/bash
# Compile official FLTube 2.1.2 as i686 ELF. FLT-01.
set -euo pipefail
SRC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/fltube-src
OUT=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/tcz-fltube
# Headers from Ubuntu (arch-independent-ish) + link Tiny Core i686 .so
if ! dpkg --print-foreign-architectures 2>/dev/null | grep -q i386; then
  sudo dpkg --add-architecture i386
  sudo apt-get update -qq
fi
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  g++-multilib libfltk1.3-dev libcurl4-openssl-dev \
  squashfs-tools >/tmp/apt-fltube.log
# curl headers (Ubuntu)
[[ -f /usr/include/curl/curl.h ]] || sudo apt-get install -y -qq libcurl4-openssl-dev
which g++
g++ -m32 -dumpmachine || true
# Unsquash Tiny Core fltk + curl + X for correct runtime ABI
POC=/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore
LIB=/tmp/fltube-tclibs
rm -rf "$LIB"
mkdir -p "$LIB"
for t in fltk-1.3 curl libX11 libXext libXft libXrender fontconfig freetype libpng libjpeg-turbo openssl; do
  for dir in "$POC/tcz" "$POC/artifacts/MiniOS-live/image/cde/optional" \
             /mnt/f/AI/xAI-projects/MiniOS/artifacts/MiniOS-live/image/cde/optional \
             "$POC/tcz-fltube"; do
    if [[ -f "$dir/${t}.tcz" ]]; then
      unsquashfs -f -d "$LIB/$t" "$dir/${t}.tcz" >/dev/null
      echo "unsquash $t from $dir"
      break
    fi
  done
done
# Fetch fltk-1.3-dev for headers if Ubuntu headers mismatch
if [[ ! -f "$POC/tcz-fltube/fltk-1.3-dev.tcz" ]]; then
  wget -q -O "$POC/tcz-fltube/fltk-1.3-dev.tcz" \
    http://tinycorelinux.net/16.x/x86/tcz/fltk-1.3-dev.tcz || true
fi
if [[ -f "$POC/tcz-fltube/fltk-1.3-dev.tcz" ]]; then
  unsquashfs -f -d "$LIB/fltk-dev" "$POC/tcz-fltube/fltk-1.3-dev.tcz" >/dev/null || true
fi
find "$LIB" -name 'libfltk*' -o -name 'FL' -type d | head
cd "$SRC"
mkdir -p build
# Include paths
INC="-Iinclude -I/usr/include -I/usr/include/x86_64-linux-gnu"
[[ -d $LIB/fltk-dev/usr/local/include ]] && INC="$INC -I$LIB/fltk-dev/usr/local/include"
# Tiny Core curl-dev if present
if [[ ! -f "$POC/tcz-fltube/curl-dev.tcz" ]]; then
  wget -q -O "$POC/tcz-fltube/curl-dev.tcz" http://tinycorelinux.net/16.x/x86/tcz/curl-dev.tcz || true
fi
if [[ -f "$POC/tcz-fltube/curl-dev.tcz" ]]; then
  unsquashfs -f -d "$LIB/curl-dev" "$POC/tcz-fltube/curl-dev.tcz" >/dev/null || true
  [[ -d $LIB/curl-dev/usr/local/include ]] && INC="$INC -I$LIB/curl-dev/usr/local/include"
fi
LPATHS=""
for d in "$LIB"/*/usr/local/lib; do
  [[ -d "$d" ]] && LPATHS="$LPATHS -L$d"
done
CXX=g++
for src in fltube_utils gnugettext_utils FLTube_View FLTube configuration_manager userdata_manager ytdlp_helper cache custom_widgets; do
  echo "CC $src"
  $CXX -m32 -std=c++17 -O2 -fexceptions $INC -DVERSION_STRING='"2.1.2"' \
    -c src/${src}.cxx -o build/${src}.o
done
RPATH=""
for d in "$LIB"/*/usr/local/lib; do
  [[ -d "$d" ]] && RPATH="$RPATH -Wl,-rpath-link,$d"
done
# Extra runtime libs for link
for t in libXcursor libxcb libXau libXdmcp libzstd; do
  for dir in /mnt/f/AI/xAI-projects/MiniOS/artifacts/MiniOS-live/image/cde/optional \
             "$POC/tcz" "$POC/tcz-fltube"; do
    if [[ -f "$dir/${t}.tcz" ]]; then
      unsquashfs -f -d "$LIB/$t" "$dir/${t}.tcz" >/dev/null || true
      break
    fi
  done
done
LPATHS=""
RPATH=""
for d in "$LIB"/*/usr/local/lib; do
  [[ -d "$d" ]] || continue
  LPATHS="$LPATHS -L$d"
  RPATH="$RPATH -Wl,-rpath-link,$d"
done
# Prefer static FLTK from -dev to get C++ typeinfo
FLTKA="$LIB/fltk-dev/usr/local/lib"
# Host 32-bit zlib if present
for z in /usr/lib/i386-linux-gnu /lib/i386-linux-gnu /usr/lib32; do
  [[ -d "$z" ]] && LPATHS="$LPATHS -L$z"
done
$CXX -m32 -o build/fltube build/*.o \
  "$FLTKA/libfltk_images.a" "$FLTKA/libfltk.a" \
  $LPATHS $RPATH \
  -lcurl -lXcursor -lXft -lXrender -lXext -lX11 \
  -lfontconfig -lfreetype -lpng -ljpeg \
  -lxcb -lXau -lXdmcp -lzstd -lz -lpthread -lstdc++ || {
    sudo apt-get install -y -qq zlib1g-dev:i386 || sudo apt-get install -y -qq zlib1g:i386
    $CXX -m32 -o build/fltube build/*.o \
      "$FLTKA/libfltk_images.a" "$FLTKA/libfltk.a" \
      $LPATHS -L/usr/lib/i386-linux-gnu -L/lib/i386-linux-gnu $RPATH \
      -lcurl -lXcursor -lXft -lXrender -lXext -lX11 \
      -lfontconfig -lfreetype -lpng -ljpeg \
      -lxcb -lXau -lXdmcp -lzstd -lz -lpthread -lstdc++
  }
file build/fltube
ls -lh build/fltube
mkdir -p "$OUT"
cp -f build/fltube "$OUT/fltube"
echo COMPILE_FLTUBE_OK
