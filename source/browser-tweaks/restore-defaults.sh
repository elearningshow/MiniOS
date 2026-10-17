#!/bin/sh
# Remove Nightly low-memory tweaks (BROWSER-PERF-AP-01)

PROFILE_DIR="$HOME/.mozilla/firefox"
DEFAULT_PROFILE=$(find "$PROFILE_DIR" -maxdepth 1 -type d -name "*.default*" | head -1)

if [ -z "$DEFAULT_PROFILE" ]; then
  echo "Could not find profile directory."
  exit 1
fi

if [ -f "$DEFAULT_PROFILE/user.js" ]; then
  rm -f "$DEFAULT_PROFILE/user.js"
  echo "Removed user.js – Nightly will use default preferences on next start."
else
  echo "No user.js found – nothing to restore."
fi
