#!/bin/bash
# Build-time: produce proprietary b43 firmware for BCM4318 (ucode5.fw etc.)
set -euo pipefail
OUT="${1:-/var/tmp/b43-fw-out}"
BASE=http://tinycorelinux.net/16.x/x86/tcz
WORK=/var/tmp/b43-fw-build
SRC_NAME=broadcom-wl-5.100.138.tar.bz2
FETCH_SCRIPT="$(cd "$(dirname "$0")" && pwd)/fetch-broadcom-wl.sh"

rm -rf "$WORK"
mkdir -p "$WORK" "$OUT"
cd "$WORK"

echo "===== fetch broadcom-wl ====="
bash "$FETCH_SCRIPT" "$WORK/$SRC_NAME"
tar xjf "$SRC_NAME"
OBJ="$(find . -name 'wl_apsta.o' | head -1)"
[[ -n "$OBJ" && -f "$OBJ" ]] || { echo "ERROR: wl_apsta.o not found"; exit 1; }
echo "OBJ=$OBJ"

echo "===== obtain b43-fwcutter (host preferred) ====="
CUTTER=""
if command -v b43-fwcutter >/dev/null 2>&1; then
  CUTTER="$(command -v b43-fwcutter)"
else
  apt-get install -y -qq b43-fwcutter >/dev/null 2>&1 || true
  if command -v b43-fwcutter >/dev/null 2>&1; then
    CUTTER="$(command -v b43-fwcutter)"
  fi
fi

if [[ -z "$CUTTER" ]]; then
  # Fall back to TinyCore 32-bit binary via qemu-i386
  wget -q -O b43-fwcutter.tcz "$BASE/b43-fwcutter.tcz"
  unsquashfs -d cut b43-fwcutter.tcz >/dev/null
  TC_CUT="$WORK/cut/usr/local/bin/b43-fwcutter"
  chmod +x "$TC_CUT"
  if command -v qemu-i386 >/dev/null 2>&1; then
    # May need i386 loader; try plain then with -L
    if qemu-i386 "$TC_CUT" -V >/dev/null 2>&1; then
      CUTTER="qemu-i386 $TC_CUT"
    elif qemu-i386 -L /usr/i686-linux-gnu "$TC_CUT" -V >/dev/null 2>&1; then
      CUTTER="qemu-i386 -L /usr/i686-linux-gnu $TC_CUT"
    fi
  fi
fi

[[ -n "$CUTTER" ]] || { echo "ERROR: no working b43-fwcutter"; exit 1; }
echo "CUTTER=$CUTTER"
# shellcheck disable=SC2086
$CUTTER -V || true

echo "===== cut firmware into $OUT ====="
rm -rf "$OUT/b43"
# shellcheck disable=SC2086
$CUTTER -w "$OUT" "$OBJ"
ls -la "$OUT/b43" | head -50 || true
test -f "$OUT/b43/ucode5.fw"
echo "B43_FIRMWARE_OK $(ls "$OUT/b43" | wc -l) files"
