#!/bin/bash
# Download Dillo (Xvesa-safe) and optional NetSurf trees. Not added to onboot.
set -euo pipefail
BASE=http://repo.tinycorelinux.net/16.x/x86/tcz
DEST="${1:-/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/tcz-browser}"
mkdir -p "$DEST"

fetch() {
  local n="${1%.tcz}.tcz"
  if [[ ! -f "$DEST/$n" ]]; then
    echo "GET $n"
    wget -q -O "$DEST/$n" "$BASE/$n" || { rm -f "$DEST/$n"; echo "FAIL $n"; return 1; }
    wget -q -O "$DEST/${n}.dep" "$BASE/${n}.dep" 2>/dev/null || true
  fi
}

declare -A SEEN
QUEUE=(dillo.tcz netsurf.tcz)
while [[ ${#QUEUE[@]} -gt 0 ]]; do
  item="${QUEUE[0]}"; QUEUE=("${QUEUE[@]:1}")
  item="${item%.tcz}.tcz"
  [[ -n "${SEEN[$item]:-}" ]] && continue
  SEEN[$item]=1
  fetch "$item" || continue
  depf="$DEST/${item}.dep"
  if [[ -f "$depf" ]]; then
    while read -r dep; do
      [[ -z "$dep" ]] && continue
      QUEUE+=("${dep%.tcz}.tcz")
    done <"$depf"
  fi
done
echo "browser packages: ${#SEEN[@]}"
ls -lh "$DEST"/*.tcz | awk '{print $5, $9}'
