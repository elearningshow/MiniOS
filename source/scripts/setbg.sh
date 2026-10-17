#!/bin/sh
# Working desktop painter from before v0.2.156. Do not stretch the picture.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export DISPLAY="${DISPLAY:-:0}"
IMG=/opt/minios/mini-os-bg.jpg
LOG=/tmp/minios-poc-wallpaper.log
{
  echo "setbg $(date) DISPLAY=$DISPLAY"
  if command -v feh >/dev/null 2>&1 && feh --no-fehbg --bg-scale "$IMG"; then
    echo "SUCCESS feh --bg-scale $IMG"
    touch /tmp/minios-wallpaper.ok
    exit 0
  fi
  if command -v hsetroot >/dev/null 2>&1 && hsetroot -fill "$IMG"; then
    echo "SUCCESS hsetroot -fill $IMG"
    touch /tmp/minios-wallpaper.ok
    exit 0
  fi
  exit 1
} >>"$LOG" 2>&1
