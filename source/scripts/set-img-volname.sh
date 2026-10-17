#!/bin/bash
# Set the FAT32 volume label on a MiniOS .img to match the version.
# FAT labels are at most 11 characters and cannot contain dots.
#   v0.2.26 / MiniOS-TC-v0.2.26-yt-open  ->  MINIOS-226
set -euo pipefail

usage() {
  echo "usage: set-img-volname.sh IMAGE.img [VERSION]" >&2
  echo "  VERSION default is taken from the file name (v0.2.26-...)." >&2
  exit 1
}

IMG="${1:-}"
VER_IN="${2:-}"
[[ -n "$IMG" && -f "$IMG" ]] || usage
[[ "$(id -u)" -eq 0 ]] || { echo "Run as root"; exit 1; }

if [[ -z "$VER_IN" ]]; then
  VER_IN=$(basename "$IMG")
fi

# Pull 0.MAJOR.MINOR (optional leading v).
if [[ "$VER_IN" =~ [vV]?([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
  MAJ="${BASH_REMATCH[1]}"
  MIN="${BASH_REMATCH[2]}"
  PAT="${BASH_REMATCH[3]}"
else
  echo "cannot parse version from: $VER_IN" >&2
  exit 1
fi

# MINIOS-226 for 0.2.26. Minor/patch must stay 1+2 digits to fit 11 chars.
LABEL=$(printf 'MINIOS-%d%02d' "$MIN" "$PAT")
if [[ "$MAJ" != 0 ]]; then
  LABEL=$(printf 'M%d-%d%02d' "$MAJ" "$MIN" "$PAT")
fi
if [[ "${#LABEL}" -gt 11 ]]; then
  echo "derived label too long: $LABEL" >&2
  exit 1
fi

command -v fatlabel >/dev/null 2>&1 || { echo "need fatlabel"; exit 1; }

LO=$(losetup -fP --show "$IMG")
cleanup() { set +e; losetup -d "$LO" 2>/dev/null; }
trap cleanup EXIT
PART="${LO}p1"
[[ -b "$PART" ]] || { echo "no partition $PART"; exit 1; }

fatlabel "$PART" "$LABEL"
GOT=$(fatlabel "$PART" | tr -d '[:space:]')
echo "VOLNAME $IMG -> $GOT (wanted $LABEL)"
[[ "$GOT" == "$LABEL" ]] || { echo "label mismatch"; exit 1; }
