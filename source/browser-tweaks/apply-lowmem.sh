#!/bin/sh
# Apply Nightly low-memory tweaks (BROWSER-PERF-AP-01)
# Run this from Terminal while Nightly is closed.

PROFILE_DIR="$HOME/.mozilla/firefox"
PREF_FILE="$(dirname "$0")/nightly-lowmem.js"

if [ ! -f "$PREF_FILE" ]; then
  echo "Preference file not found: $PREF_FILE"
  exit 1
fi

# Locate the default profile (simple heuristic)
DEFAULT_PROFILE=$(find "$PROFILE_DIR" -maxdepth 1 -type d -name "*.default*" | head -1)

if [ -z "$DEFAULT_PROFILE" ]; then
  echo "Could not find Nightly/Firefox profile directory."
  echo "You can manually copy the preferences from nightly-lowmem.js into about:config."
  exit 1
fi

echo "Found profile: $DEFAULT_PROFILE"
cp "$PREF_FILE" "$DEFAULT_PROFILE/user.js"
echo "Low-memory preferences written to $DEFAULT_PROFILE/user.js"
echo "Start Nightly for the changes to take effect."
echo "To remove the tweaks later, delete the user.js file or run the restore script."
