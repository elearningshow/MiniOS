#!/bin/bash
# Download Firefox ESR 140 + deps for Tiny Core 16.x x86. Not added to onboot.
set -euo pipefail
BASE=http://repo.tinycorelinux.net/16.x/x86/tcz
DEST="${1:-/mnt/f/AI/xAI-projects/MiniOS/poc/tinycore/tcz-firefox}"
mkdir -p "$DEST"

fetch() {
  local n="${1%.tcz}.tcz"
  if [[ ! -f "$DEST/$n" ]]; then
    echo "GET $n"
    wget -q -O "$DEST/$n" "$BASE/$n" || { rm -f "$DEST/$n"; echo "FAIL $n"; return 1; }
    wget -q -O "$DEST/${n}.info" "$BASE/${n}.info" 2>/dev/null || true
    wget -q -O "$DEST/${n}.dep" "$BASE/${n}.dep" 2>/dev/null || true
    wget -q -O "$DEST/${n}.md5.txt" "$BASE/${n}.md5.txt" 2>/dev/null || true
  fi
}

declare -A SEEN
QUEUE=(firefox-ESR.tcz dejavu-fonts-ttf.tcz)
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

echo "firefox-ESR packages: ${#SEEN[@]}"
du -sh "$DEST"
ls -lh "$DEST"/firefox-ESR.tcz
test -f "$DEST/firefox-ESR.tcz"
grep -q firefox-ESR <<<"${!SEEN[*]}"
