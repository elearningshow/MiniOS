#!/bin/sh
echo "Loading MiniOS"
# bootsync runs before startx. bootlocal is backgrounded after this.
/opt/minios/minios-fix-tcedir >/tmp/minios-fix-tcedir.log 2>&1 || true
# Xvesa -nozap must be in place before the desktop starts.
[ -x /opt/minios/minios-xvesa-install ] && /opt/minios/minios-xvesa-install
/usr/bin/sethostname box
/opt/bootlocal.sh &
# Play mini-mini.mp3 once, then unmount that player tree. Do not drop_caches
# (that raced Wi-Fi). Do not change the Wi-Fi dock.
if [ -x /opt/minios/minios-sound-up ]; then
  /opt/minios/minios-sound-up boot-chime >>/tmp/minios-sound.log 2>&1 || true
fi
