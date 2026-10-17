#!/bin/sh
# Early USB log first so Windows can read minios-logs\LATEST.txt even if X fails.
if [ -x /opt/minios/minios-log-usb ]; then
  /opt/minios/minios-log-usb boot-early >/tmp/minios-boot-early.log 2>&1 || true
fi

# Swap file first, before X. Creating it after the desktop started
# made the navigation dock disappear (wbar died under the write).
if [ -x /opt/minios/minios-swap-early ]; then
  /opt/minios/minios-swap-early
fi
if [ -x /opt/minios/minios-xvesa-nozap ]; then
  /opt/minios/minios-xvesa-nozap
fi
# Keep trying until startx has created ~/.xsession (bootlocal is backgrounded).
(
  i=0
  while [ "$i" -lt 30 ]; do
    i=$((i + 1))
    [ -x /opt/minios/minios-xvesa-nozap ] && /opt/minios/minios-xvesa-nozap
    [ -e /tmp/.X11-unix/X0 ] && break
    sleep 1
  done
  [ -x /opt/minios/minios-log-usb ] && /opt/minios/minios-log-usb boot >/dev/null 2>&1 || true
) >/tmp/minios-bootlocal-nozap.log 2>&1 &

# AP-06: fix home, wallpaper, wireless firmware. Never browser.
[ -x /opt/minios/fix-home.sh ] && /opt/minios/fix-home.sh

( i=0
  while [ "$i" -lt 40 ]; do
    i=$((i + 1))
    sleep 2
    if [ -e /tmp/.X11-unix/X0 ]; then
      DISPLAY=:0 /opt/minios/setbg.sh && break
    fi
  done ) >/tmp/minios-bootlocal-bg.log 2>&1 &

( sleep 5
  if [ -d /usr/local/lib/firmware/b43 ] && [ ! -f /lib/firmware/b43/ucode5.fw ]; then
    mkdir -p /lib/firmware
    ln -sfn /usr/local/lib/firmware/b43 /lib/firmware/b43
    ln -sfn /usr/local/lib/firmware/b43-open /lib/firmware/b43-open 2>/dev/null
  fi
  modprobe cfg80211 2>/dev/null
  modprobe mac80211 2>/dev/null
  modprobe ssb 2>/dev/null
  modprobe bcma 2>/dev/null
  modprobe -r b43 2>/dev/null
  sleep 1
  modprobe b43 2>/dev/null
) >/tmp/minios-bootlocal-wifi.log 2>&1 &
