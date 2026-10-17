#!/bin/sh
# Put Games, then AI, immediately left of Wi-Fi, then start the bar.
# Other icons stay in the order already on the list.
ICONS="${MINIOS_ICONS:-/usr/local/tce.icons}"
AI_I='i: /opt/minios/ai.png'
AI_T='t: AI'
AI_C='c: exec /usr/local/bin/minios-run ai -- /usr/local/bin/minios-ai-chat'
GM_I='i: /opt/minios/games.png'
GM_T='t: Games'
GM_C='c: exec /usr/local/bin/minios-games'
# Build the icon list here, then draw the bar once.
if [ "$MINIOS_WBAR_TEST" != 1 ]; then
  i=0
  while [ ! -f "$ICONS" ] && [ "$i" -lt 30 ]; do
    sleep 1
    i=$((i + 1))
  done
  if [ ! -f /tmp/minios-dock.ready ] && [ -x /opt/minios/swap-wbar.sh ]; then
    /opt/minios/swap-wbar.sh
  fi
  bar_n=0
  while [ ! -f /tmp/minios-dock.ready ] && [ "$bar_n" -lt 45 ]; do
    sleep 1
    bar_n=$((bar_n + 1))
  done
  if ! pidof wbar >/dev/null 2>&1; then
    nohup wbar >/dev/null 2>&1 &
  fi
  exit 0
fi
added=0
need=0
if [ -f "$ICONS" ]; then
  grep -q 'minios-ai-chat' "$ICONS" 2>/dev/null || need=1
  grep -q 'minios-games' "$ICONS" 2>/dev/null || need=1
fi
if [ -f "$ICONS" ] && [ "$need" -eq 1 ]; then
  tmp=/tmp/tce.icons.ai
  : >"$tmp"
  icon=""
  title=""
  saw=0
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      'i: '*) icon=$line ;;
      't: '*) title=$line ;;
      'c: '*)
        case "$line" in
          *minios-ai*)
            if [ -n "$icon" ] && [ -n "$title" ]; then
              AI_I=$icon; AI_T=$title; AI_C=$line
            fi
            ;;
          *minios-games*)
            if [ -n "$icon" ] && [ -n "$title" ]; then
              GM_I=$icon; GM_T=$title; GM_C=$line
            fi
            ;;
          *wifi.sh*|*minios-wifi*)
            saw=1
            printf '%s\n%s\n%s\n' "$GM_I" "$GM_T" "$GM_C" >>"$tmp"
            printf '%s\n%s\n%s\n' "$AI_I" "$AI_T" "$AI_C" >>"$tmp"
            printf '%s\n%s\n%s\n' "$icon" "$title" "$line" >>"$tmp"
            ;;
          *)
            if [ -n "$icon" ] && [ -n "$title" ]; then
              printf '%s\n%s\n%s\n' "$icon" "$title" "$line" >>"$tmp"
            fi
            ;;
        esac
        icon=""
        title=""
        ;;
    esac
  done <"$ICONS"
  if [ "$saw" -eq 0 ]; then
    printf '%s\n%s\n%s\n' "$GM_I" "$GM_T" "$GM_C" >>"$tmp"
    printf '%s\n%s\n%s\n' "$AI_I" "$AI_T" "$AI_C" >>"$tmp"
  fi
  if cp "$tmp" "$ICONS" 2>/dev/null || sudo cp "$tmp" "$ICONS" 2>/dev/null; then
    added=1
  fi
  rm -f "$tmp"
fi
if [ "$MINIOS_WBAR_TEST" = 1 ]; then
  exit 0
fi
if pidof wbar >/dev/null 2>&1; then
  # The bar already read the list without the new button. Restart once.
  if [ "$added" -eq 1 ]; then
    killall wbar >/dev/null 2>&1 || true
    nohup wbar >/dev/null 2>&1 &
  fi
  exit 0
fi
nohup wbar >/dev/null 2>&1 &
exit 0
