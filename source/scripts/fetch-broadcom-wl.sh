#!/bin/bash
set -euo pipefail
OUT="${1:-/tmp/broadcom-wl-5.100.138.tar.bz2}"
cd "$(dirname "$OUT")"
NAME="$(basename "$OUT")"
for u in \
  "https://sources.cdn.openwrt.org/broadcom-wl-5.100.138.tar.bz2" \
  "https://mirror2.openwrt.org/sources/broadcom-wl-5.100.138.tar.bz2" \
  "https://downloads.openwrt.org/sources/broadcom-wl-5.100.138.tar.bz2" \
  "http://mirror.math.princeton.edu/pub/openwrt/sources/broadcom-wl-5.100.138.tar.bz2" \
  "https://archive.kernel.org/old-cdn/software/network/wireless/broadcom-wl-5.100.138.tar.bz2"
do
  echo "TRY $u"
  if wget -q -O "$NAME" "$u"; then
    if file "$NAME" | grep -qiE 'bzip|gzip|compress|tar|data'; then
      ls -lh "$NAME"
      file "$NAME"
      echo "OK_SOURCE=$u"
      exit 0
    fi
  fi
  rm -f "$NAME"
done
echo "FAIL: no mirror"
exit 1
