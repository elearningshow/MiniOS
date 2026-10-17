#!/bin/bash
# Build the next MiniOS image from this GitHub package.
# Base image: MiniOS-TC-v0.2.260-bar.img in the folder above this script.
# Output: ../out/MiniOS-TC-v0.2.261-edit.img and a matching .copy
# Does not overwrite the packaged image.
# Run from WSL as root: bash bake/build-next-image.sh
# Edit source/scripts first. Checksums in this file refuse a changed player.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJ="$ROOT"
POC="$ROOT/source"
export SCRIPTS="$ROOT/source/scripts"
SRC="$ROOT/MiniOS-TC-v0.2.260-bar.img"
VER=v0.2.261-edit
IMG_NAME=MiniOS-TC-v0.2.261-edit.img
CAPTION=MiniOS-TC-v0.2.261
IMG_OUT="$ROOT/out/$IMG_NAME"
BACKUP_IMG="$ROOT/out/${IMG_NAME}.copy"
mkdir -p "$ROOT/out"
if [[ "$IMG_OUT" == "$SRC" || "$BACKUP_IMG" == "$SRC" ]]; then
  echo "FAIL refuses to overwrite the packaged image"
  exit 1
fi

PLAYER_MD5=4f98713e736ac7f892d7ceccb053606e
MPLAYER_MD5=a2ecd453d68056ba8ed0b50cc1108082
PLAY_MD5=438d1eb48af8b76ac7e1c59716820a63
TTMP_MD5=c36913872297871221d067dab640cd07
THUMB_MD5=01cc57b5e6f1e098c6ca5bd0be658887
SEARCH_MD5=2bae95cb804e90d1103c4931910e8fd8
TTBAR_MD5=1ac1e77d242a1798292ef7df9badc4cc
GUI_MD5=a44905480fa1e7912b590f9ddee00c55
REL_MD5=1598bda8551dc2990c28ccac3e3518b0
RESOLVE_MD5=9a64d8147e356bf49d2c9b2b1043b842
ARM_MD5=0679a6c4798416c3b9795ddcd069233c
NOW_MD5=76f301f45652bc5bfef16e3cd05e0bd2
GUARD_MD5=4a241797cd6e8e340309a6b797a71ed3
NEW_BAR_LOCK=1ac1e77d242a1798292ef7df9badc4cc
NEW_GUI_LOCK=a44905480fa1e7912b590f9ddee00c55
NEW_SEARCH_LOCK=2bae95cb804e90d1103c4931910e8fd8
NEW_TTMP_LOCK=c36913872297871221d067dab640cd07
NEW_PLAY_LOCK=438d1eb48af8b76ac7e1c59716820a63

tt_quiet_ok() {
  python3 - "$1" <<'PY'
import sys
from pathlib import Path
t = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
start = t.find("tt_play_quiet_picture() {")
if start < 0:
    raise SystemExit("FAIL quiet picture missing")
rest = t[start:]
end = rest.find("\n}\n")
body = rest[:end]
for bad in ("lowres", "framedrop", "-fps", "-correct-pts", "-autosync", "-speed", "-mc ", "-osdlevel 1"):
    if bad in body:
        raise SystemExit("FAIL quiet picture has %s" % bad)
if "-osdlevel 0" not in body:
    raise SystemExit("FAIL quiet picture still draws on the video")
if "-demuxer lavf" not in body or "/tmp/minios-tt-mp.cmd" not in body:
    raise SystemExit("FAIL quiet picture lost the reader")
if "tt_stop_when_closed" not in body or "tt_disarm_close_kill" not in body:
    raise SystemExit("FAIL quiet picture does not close")
tail = rest[end + 3:]
if not tail.lstrip().startswith("tt_play_quiet_picture\n"):
    raise SystemExit("FAIL quiet picture is not called")
if "\ntt_play_sharp_picture\n" in t:
    raise SystemExit("FAIL sharp picture is still called")
if "\ntt_play_picture_with_sound\n" in t:
    raise SystemExit("FAIL half-size player is still called")
if "tt_play_sharp_picture() {" not in t:
    raise SystemExit("FAIL sharp picture was removed")
print("QUIET_OK")
PY
}

echo "STARTING_IMG=$SRC PROJECT=$VER"
[[ "$(id -u)" -eq 0 ]] || { echo "Run as root"; exit 1; }
[[ -f "$SRC" ]] || { echo "Missing image"; exit 1; }
[[ ! -e "$IMG_OUT" && ! -e "$BACKUP_IMG" ]] || { echo "FAIL output image already exists"; exit 1; }
if grep -q 'bg-fill' "$SCRIPTS/setbg.sh"; then echo "FAIL setbg stretches"; exit 1; fi
grep -q 'bg-scale' "$SCRIPTS/setbg.sh"
sed -i 's/\r$//' "$SCRIPTS/minios-tiktok-play" "$SCRIPTS/minios-tiktok-search" "$SCRIPTS/minios-tt-mplayer" "$SCRIPTS/minios-tiktok-thumbs"
sh -n "$SCRIPTS/minios-tiktok-play"
sh -n "$SCRIPTS/minios-tiktok-search"
sh -n "$SCRIPTS/minios-tt-mplayer"
sh -n "$SCRIPTS/minios-tiktok-thumbs"
sed -i 's/\r$//' "$SCRIPTS/bootsync.sh" "$SCRIPTS/minios-swap-wbar" "$SCRIPTS/wbar-wait.sh" "$SCRIPTS/minios-streaming-play" "$SCRIPTS/minios-streaming-search" "$SCRIPTS/minios-run" "$SCRIPTS/wmx-Document" "$SCRIPTS/wmx-Spreadsheet"
sh -n "$SCRIPTS/bootsync.sh"
sh -n "$SCRIPTS/minios-swap-wbar"
sh -n "$SCRIPTS/wbar-wait.sh"
grep -q 'Loading MiniOS' "$SCRIPTS/bootsync.sh"
if grep -q 'live desktop' "$SCRIPTS/bootsync.sh"; then echo "FAIL old boot line"; exit 1; fi
grep -q 'keep_streaming_on_bar' "$SCRIPTS/minios-swap-wbar"
grep -q 'start_bar_after_icons' "$SCRIPTS/minios-swap-wbar"
grep -q 'minios-dock.ready' "$SCRIPTS/wbar-wait.sh"
if grep -q 'st_wait' "$SCRIPTS/wbar-wait.sh"; then echo "FAIL bar still waits"; exit 1; fi
grep -q 'bar_n' "$SCRIPTS/wbar-wait.sh"
if grep -q 'killall wbar' "$SCRIPTS/minios-swap-wbar"; then echo "FAIL second bar"; exit 1; fi
grep -q 'stream-v252 full' "$SCRIPTS/minios-streaming.c"
grep -q '<Previous' "$SCRIPTS/minios-streaming.c"
grep -q 'st_ready_player' "$SCRIPTS/minios-streaming-play"
grep -q 'st_ready_player || exit 1' "$SCRIPTS/minios-streaming-play"
grep -q 'lastcheckok' "$SCRIPTS/minios-streaming-search"
grep -a -q 'stream-v252 full' "$SCRIPTS/minios-streaming"
grep -q 'stream-v253 pop' "$SCRIPTS/minios-streaming.c"
grep -q 'Loading the station. Please wait.' "$SCRIPTS/minios-streaming.c"
grep -q 'minios-st-cmd' "$SCRIPTS/minios-streaming-play"
grep -a -q 'stream-v253 pop' "$SCRIPTS/minios-streaming"
grep -q 'stream-v254 kind' "$SCRIPTS/minios-streaming.c"
grep -q 'st_media_kind' "$SCRIPTS/minios-streaming.c"
grep -q 'Each row is marked Video or Audio.' "$SCRIPTS/minios-streaming-search"
grep -a -q 'stream-v254 kind' "$SCRIPTS/minios-streaming"
grep -q 'stream-v256 view' "$SCRIPTS/minios-streaming.c"
grep -q 'stream-v256 row' "$SCRIPTS/minios-streaming.c"
grep -q 'st_paint_audio_row' "$SCRIPTS/minios-streaming.c"
grep -a -q 'stream-v256 view' "$SCRIPTS/minios-streaming"
grep -a -q 'stream-v256 row' "$SCRIPTS/minios-streaming"
grep -q 'minios-st-geom' "$SCRIPTS/minios-streaming-play"
grep -q 'st_exec_picture' "$SCRIPTS/minios-streaming-play"
grep -q -- '-noborder' "$SCRIPTS/minios-streaming-play"
grep -q -- '-geometry' "$SCRIPTS/minios-streaming-play"
grep -q 'Video stations are included' "$SCRIPTS/minios-streaming-search"
grep -q 'video source' "$SCRIPTS/minios-streaming-search"
grep -q 'iptv-org.github.io' "$SCRIPTS/minios-streaming-search"
grep -q 'plutotv_us.m3u' "$SCRIPTS/minios-streaming-search"
grep -q 'playlist_usa.m3u8' "$SCRIPTS/minios-streaming-search"
grep -q 'More than one video list was searched' "$SCRIPTS/minios-streaming-search"
if grep -q -- '-wid' "$SCRIPTS/minios-streaming-play"; then echo 'FAIL picture uses -wid'; exit 1; fi
grep -q 'tt_sync_clip=$(tt_make_clear_file' "$SCRIPTS/minios-tiktok-play"
if grep -q 'tt_sync_clip=$(tt_make_sync_file' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL still calls the soft picture"; exit 1; fi
grep -q 'wait "$tt_clear_pid"' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_clear_pid=""' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_sync_file_ok "$tt_sync_clip"' "$SCRIPTS/minios-tiktok-play"
grep -q 'wait "$tt_sync_pid"' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_sync_pid=""' "$SCRIPTS/minios-tiktok-play"
grep -q '/tmp/minios-tt-sync.pid' "$SCRIPTS/minios-tiktok-play"
grep -F -q 'cap = 426 * 240' "$SCRIPTS/minios-tiktok-play"
grep -F -q 'if(gt(iw,ih),240,-2)' "$SCRIPTS/minios-tiktok-play"
grep -F -q 'if(gt(iw,ih),320,-2)' "$SCRIPTS/minios-tiktok-play"
grep -F -q 'flags=lanczos' "$SCRIPTS/minios-tiktok-play"
grep -F -q -- '-q:v 3' "$SCRIPTS/minios-tiktok-play"
grep -F -q -- '-bf 0' "$SCRIPTS/minios-tiktok-play"
grep -q -- '-c:v mpeg4' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_load_picture_libs' "$SCRIPTS/minios-tiktok-play"
grep -q 'libavdevice4' "$SCRIPTS/minios-tiktok-play"
if grep -q 'tt_small_clip=$(tt_make_light_file' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL still calls the unchecked file"; exit 1; fi
if grep -q 'play saved file' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL still plays the large file"; exit 1; fi
if grep -q 'tt_small_clip=$(tt_fit_one_core' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL still refuses the saved file"; exit 1; fi
if grep -q 'tt_small_clip=$(tt_shrink_picture' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL still calls the old shrink"; exit 1; fi
if grep -q 'This video is too heavy for this laptop.' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL too-heavy refusal remains"; exit 1; fi
if grep -q 'framedrop' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL play script skips pictures"; exit 1; fi
if grep -q -- '-fps' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL play script forces fps"; exit 1; fi
if grep -q -- '-correct-pts' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL play script correct-pts"; exit 1; fi
if grep -q -- '-autosync' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL play script autosync"; exit 1; fi
grep -q 'tt_pick_budget' "$SCRIPTS/minios-tiktok-search"
grep -q 'tt_budget_up' "$SCRIPTS/minios-tiktok-search"
grep -q 'tt_pick_budget "$TT_BUDGET_ARG"' "$SCRIPTS/minios-tiktok-search"
grep -q 'tt_cycle_search_time' "$SCRIPTS/minios-tiktok.c"
grep -q 'tt_cycle_search_time();' "$SCRIPTS/minios-tiktok.c"
grep -q 'Search runs for %d seconds, then shows the list.' "$SCRIPTS/minios-tiktok.c"
grep -q 'minios-tiktok-search", query, secbuf' "$SCRIPTS/minios-tiktok.c"
grep -q 'tt_take_play_lock || exit 1' "$SCRIPTS/minios-tiktok-play"
grep -q 'play lock stop' "$SCRIPTS/minios-tiktok-play"
grep -q 'rm -f /tmp/minios-ttplay.mp4' "$SCRIPTS/minios-tiktok-play"
if grep -q 'second play skipped' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL playlist play still skipped"; exit 1; fi
grep -q '^MP=/opt/minios/minios-tt-mplayer$' "$SCRIPTS/minios-tiktok-play"
grep -q 'MP=/usr/local/bin/minios-tt-mplayer' "$SCRIPTS/minios-tiktok-play"
if grep -q 'minios-tt-mplaye$' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL short helper name"; exit 1; fi
grep -q 'best = inner' "$SCRIPTS/minios-tiktok-play"
if grep -q 'best = inne$' "$SCRIPTS/minios-tiktok-play"; then echo "FAIL size reader"; exit 1; fi
grep -q 'tt_arm_close_kill' "$SCRIPTS/minios-tt-mplayer"
grep -q 'tt_disarm_close_kill' "$SCRIPTS/minios-tt-mplayer"
grep -q 'wait "$tt_guard_pid"' "$SCRIPTS/minios-tt-mplayer"
grep -q -- '-demuxer lavf' "$SCRIPTS/minios-tt-mplayer"
tt_quiet_ok "$SCRIPTS/minios-tt-mplayer"
if grep -q 'framedrop' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper skips pictures"; exit 1; fi
if grep -q -- '-fps' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper forces fps"; exit 1; fi
if grep -q -- '-correct-pts' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper correct-pts"; exit 1; fi
if grep -q -- '-autosync' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper autosync"; exit 1; fi
if grep -q -- '-speed' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper speed"; exit 1; fi
if grep -q -- '-mc ' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper mc"; exit 1; fi
grep -q -- '-input file=/tmp/minios-tt-mp.cmd' "$SCRIPTS/minios-tt-mplayer"
if grep -q -- '-input file=/tmp/minios-tt-mp.fwd' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper still reads the forward pipe"; exit 1; fi
if grep -q 'kill -STOP' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper still freezes the picture"; exit 1; fi
if grep -q 'tt_forward_player_cmds' "$SCRIPTS/minios-tt-mplayer"; then echo "FAIL wrapper still forwards commands"; exit 1; fi
grep -q 'tt_keep_open_file' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_open_clip=$(tt_keep_open_file' "$SCRIPTS/minios-tiktok-play"
grep -q '/tmp/minios-tt-open.mp4' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_stop_hidden_work' "$SCRIPTS/minios-tiktok-play"
grep -q "trap 'tt_play_cleanup; exit 0' TERM INT" "$SCRIPTS/minios-tiktok-play"
if grep -n 'tt_show_playing_name' "$SCRIPTS/minios-tiktok-play" | grep -v 'tt_show_playing_name()'; then echo "FAIL title strip is still started"; exit 1; fi
grep -q '/tmp/minios-tt-thumb-page.txt' "$SCRIPTS/minios-tiktok-thumbs"
grep -q 'origin_cover' "$SCRIPTS/minios-tiktok-thumbs"
grep -q 'tt_thumb_file' "$SCRIPTS/minios-tiktok.c"
grep -a -q 'Now playing - %s' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'pausing_keep_force seek %d 2' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v231 vol' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v232 pause-bar' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v232 play-bar' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'Paused clip %d of %d' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'Could not load the video.' "$SCRIPTS/minios-tt-player-bar"
if grep -a -q 'set_property volume' "$SCRIPTS/minios-tt-player-bar"; then echo "FAIL bar still sends a second volume"; exit 1; fi
grep -a -q 'pausing_keep_force volume %d 1' "$SCRIPTS/minios-tt-player-bar"
if grep -a -q framedrop "$SCRIPTS/minios-tt-player-bar"; then echo "FAIL bar skips pictures"; exit 1; fi
if objdump -T "$SCRIPTS/minios-tt-player-bar" | grep -E 'GLIBC_2\.(3[5-9]|[4-9][0-9])'; then echo "FAIL bar needs a newer C library"; exit 1; fi
file "$SCRIPTS/minios-tt-player-bar" | grep -q '80386'
file "$SCRIPTS/minios-tiktok" | grep -q '80386'
grep -q 'This video did not start.' "$SCRIPTS/minios-tt-mplayer"
grep -q 'MiniOS TikTok Player - Now playing' "$SCRIPTS/minios-tt-now.c"
grep -q 'XReparentWindow' "$SCRIPTS/minios-tt-now.c"
if grep -q 'shape.h' "$SCRIPTS/minios-tt-now.c"; then echo "FAIL title strip needs a missing header"; exit 1; fi
grep -q 'minios-tt-mp.quit' "$SCRIPTS/minios-tt-guard.c"
grep -q 'SIGKILL' "$SCRIPTS/minios-tt-guard.c"
if grep -q 'minios-tiktok-play' "$SCRIPTS/minios-tt-guard.c"; then echo "FAIL guard name collides"; exit 1; fi
[[ "$(md5sum "$SCRIPTS/minios-yt-resolve" | awk '{print $1}')" == "$RESOLVE_MD5" ]]
[[ "$(md5sum "$SCRIPTS/minios-yt-arm" | awk '{print $1}')" == "$ARM_MD5" ]]
[[ "$(md5sum "$SCRIPTS/minios-tt-release" | awk '{print $1}')" == "$REL_MD5" ]]
[[ "$(md5sum "$SCRIPTS/minios-tt-guard" | awk '{print $1}')" == "$GUARD_MD5" ]]
python3 - <<'PY'
from pathlib import Path
rec = Path(__import__("os").environ["SCRIPTS"], "minios-reclaim").read_text()
part = rec.split("Search-GUI close", 1)[1]
part = part.split("youtube|fltube|minios-youtube)", 1)[1].split("tiktok|minios-tiktok)", 1)
yt, rest = part[0], part[1]
tt = rest.split("*)", 1)[0]
if "reset_zram 12" not in yt:
    raise SystemExit("FAIL youtube zram branch changed")
if "reset_zram" in tt or "drop_caches" in tt:
    raise SystemExit("FAIL tiktok reclaim still resets zram")
rel = Path(__import__("os").environ["SCRIPTS"], "minios-tt-release").read_text()
if "drop_caches" in rel or "minios-appkit" in rel:
    raise SystemExit("FAIL release drops caches or unloads the kit")
if "tt-release leave python and caches" not in rel:
    raise SystemExit("FAIL release lost the python line")
print("ISOLATION_OK")
PY
NEW_PLAY_MD5=$(md5sum "$SCRIPTS/minios-tiktok-play" | awk '{print $1}')
NEW_TTMP_MD5=$(md5sum "$SCRIPTS/minios-tt-mplayer" | awk '{print $1}')
NEW_SEARCH_MD5=$(md5sum "$SCRIPTS/minios-tiktok-search" | awk '{print $1}')
NEW_GUI_MD5=$(md5sum "$SCRIPTS/minios-tiktok" | awk '{print $1}')
NEW_NOW_MD5=$(md5sum "$SCRIPTS/minios-tt-now" | awk '{print $1}')
NEW_BAR_MD5=$(md5sum "$SCRIPTS/minios-tt-player-bar" | awk '{print $1}')
NEW_THUMB_MD5=$(md5sum "$SCRIPTS/minios-tiktok-thumbs" | awk '{print $1}')
echo "NEW_PLAY_MD5=$NEW_PLAY_MD5"
echo "NEW_TTMP_MD5=$NEW_TTMP_MD5"
echo "NEW_SEARCH_MD5=$NEW_SEARCH_MD5"
echo "NEW_GUI_MD5=$NEW_GUI_MD5"
echo "NEW_NOW_MD5=$NEW_NOW_MD5"
echo "NEW_BAR_MD5=$NEW_BAR_MD5"
echo "NEW_THUMB_MD5=$NEW_THUMB_MD5"
[[ "$NEW_PLAY_MD5" == "$PLAY_MD5" ]]
[[ "$NEW_PLAY_MD5" == "$NEW_PLAY_LOCK" ]]
[[ "$NEW_TTMP_MD5" == "$TTMP_MD5" ]]
[[ "$NEW_TTMP_MD5" == "$NEW_TTMP_LOCK" ]]
[[ "$NEW_SEARCH_MD5" == "$SEARCH_MD5" ]]
[[ "$NEW_SEARCH_MD5" == "$NEW_SEARCH_LOCK" ]]
[[ "$NEW_GUI_MD5" == "$GUI_MD5" ]]
[[ "$NEW_GUI_MD5" == "$NEW_GUI_LOCK" ]]
[[ "$NEW_NOW_MD5" == "$NOW_MD5" ]]
[[ "$NEW_THUMB_MD5" == "$THUMB_MD5" ]]
[[ "$NEW_BAR_MD5" == "$TTBAR_MD5" ]]
[[ "$NEW_BAR_MD5" == "$NEW_BAR_LOCK" ]]
if grep -a -q 'Now playing' "$SCRIPTS/minios-tiktok"; then echo "FAIL search still says Now playing"; exit 1; fi
grep -q 'tt_pull_more_unseen' "$SCRIPTS/minios-tiktok-search"
grep -q 'MINIOS_TT_WATCHED' "$SCRIPTS/minios-tiktok-search"
grep -q 'if vid in watched_ids' "$SCRIPTS/minios-tiktok-search"
grep -a -q 'MINIOS_TT_WATCHED' "$SCRIPTS/minios-tiktok"
grep -a -q 'player-v233 ask' "$SCRIPTS/minios-tiktok"
grep -a -q 'search-v234 hide' "$SCRIPTS/minios-tiktok"
grep -a -q 'player-v233 list-end' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v234 unmap' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'More videos' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'New search' "$SCRIPTS/minios-tt-player-bar"
grep -q 'Downloading media' "$SCRIPTS/minios-tiktok-play"
grep -q 'Optimizing video playback' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt-high play' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt-high fell back' "$SCRIPTS/minios-tiktok-play"
grep -q 'MINIOS_TT_HD' "$SCRIPTS/minios-tiktok-play"
grep -q 'tt_use_high_file' "$SCRIPTS/minios-tiktok-play"
grep -q 'FAST_SHOW=16' "$SCRIPTS/minios-tiktok-search"
grep -q 'more videos other sites' "$SCRIPTS/minios-tiktok-search"
grep -q 'more videos saved list' "$SCRIPTS/minios-tiktok-search"
grep -q 'more next page' "$SCRIPTS/minios-tiktok-search"
grep -q 'FAIL lang' "$SCRIPTS/minios-tiktok-search"
grep -q 'search lang=' "$SCRIPTS/minios-tiktok-search"
grep -q 'same search continue' "$SCRIPTS/minios-tiktok-search"
grep -q 'short fill round' "$SCRIPTS/minios-tiktok-search"
grep -q 'fill until budget' "$SCRIPTS/minios-tiktok-search"
grep -q 'fast skip lang' "$SCRIPTS/minios-tiktok-search"
grep -q 'more until budget' "$SCRIPTS/minios-tiktok-search"
grep -q 'more keep round' "$SCRIPTS/minios-tiktok-search"
grep -q 'tt_more_keep_going' "$SCRIPTS/minios-tiktok-search"
grep -q 'fast skip seen' "$SCRIPTS/minios-tiktok-search"
grep -q 'tt_fill_short_list' "$SCRIPTS/minios-tiktok-search"
grep -q 'Press Go search! again.' "$SCRIPTS/minios-tiktok.c"
grep -a -q 'search-v245 go' "$SCRIPTS/minios-tiktok"
grep -a -q 'search-v246 same' "$SCRIPTS/minios-tiktok"
grep -q 'rowcount)" -lt 12' "$SCRIPTS/minios-tiktok-search"
grep -q 'rowcount)" -lt 8' "$SCRIPTS/minios-tiktok-search"
grep -q 'search-v235 quality' "$SCRIPTS/minios-tiktok.c"
grep -a -q 'search-v235 quality' "$SCRIPTS/minios-tiktok"
grep -a -q 'search-v238 more' "$SCRIPTS/minios-tiktok"
grep -a -q 'search-v244 same' "$SCRIPTS/minios-tiktok"
if grep -a -q 'No further videos' "$SCRIPTS/minios-tiktok"; then echo 'FAIL old more-videos sentence'; exit 1; fi
grep -a -q 'player-v235 hold' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v237 load-line' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v240 stage' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v241 title' "$SCRIPTS/minios-tt-player-bar"
grep -a -q 'player-v244 last' "$SCRIPTS/minios-tt-player-bar"
python3 - <<'PY'
from pathlib import Path
t = Path(__import__("os").environ["SCRIPTS"], "minios-tiktok-search").read_text(encoding="utf-8", errors="replace")
start = t.find("search_fast_show() {")
body = t[start:t.find("\n}\n", start)]
for bad in ("tt_keep_asking", "tt_fill_startpage", "tt_resolve_shorts", "tt_fill_unseen"):
    if bad in body:
        raise SystemExit("FAIL search_fast_show still calls %s" % bad)
for item in ("-lt 12", "-lt 8", "tt_pull_more_unseen", "tt_confirm_show", "tt_fill_short_list", "tt_more_keep_going"):
    if item not in body:
        raise SystemExit("FAIL search_fast_show lost %s" % item)
print("SEARCH_PATH_OK")
PY
grep -q 'de1.api.radio-browser.info' "$SCRIPTS/minios-streaming-search"
grep -q 'Free-TV/IPTV' "$SCRIPTS/minios-streaming-search"
grep -q 'stream-v247 search' "$SCRIPTS/minios-streaming.c"
grep -q 'st_pick_low_hls' "$SCRIPTS/minios-streaming-play"
grep -a -q 'stream-v247 search' "$SCRIPTS/minios-streaming"
grep -a -q 'stream-v247 play' "$SCRIPTS/minios-streaming"
grep -q 'minios-streaming' "$SCRIPTS/minios-swap-wbar"
grep -q 'scope streaming' "$SCRIPTS/minios-reclaim"
grep -q 'Streaming is loading' "$SCRIPTS/minios-run"
grep -q 'add_office_icons' "$SCRIPTS/minios-swap-wbar"
grep -q '^keep_streaming_on_bar$' "$SCRIPTS/minios-swap-wbar"
grep -q 'Document is loading' "$SCRIPTS/minios-run"
grep -q 'bar_n" -lt 45' "$SCRIPTS/wbar-wait.sh"
grep -q '/opt/minios/swap-wbar.sh' "$SCRIPTS/wbar-wait.sh"
grep -q 'setbg.sh' "$SCRIPTS/minios-swap-wbar"
grep -q 't: Document' "$SCRIPTS/minios-swap-wbar"
grep -q 'abiword.tcz' "$SCRIPTS/wmx-Document"
grep -q 'gnumeric.tcz' "$SCRIPTS/wmx-Spreadsheet"
echo "SCRIPT_OK"

WORK=/tmp/MiniOS-TC-${VER}.img
rm -f "$WORK"
cp -f "$SRC" "$WORK"
echo "COPY_DONE"
MNT=/tmp/minios-v261-mnt
EXT=/tmp/minios-v261-core
rm -rf "$MNT" "$EXT"
mkdir -p "$MNT" "$EXT"
cleanup() { set +e; mountpoint -q "$MNT" && umount "$MNT"; }
trap cleanup EXIT
mount -o loop,offset=1048576,rw "$WORK" "$MNT"
echo "MOUNT_OK"
for need in libavdevice4 libavfilter4 libavresample4 SDL2 libv4l2; do
  [[ -f "$MNT/cde/optional/${need}.tcz" ]]
done
echo "TCZ_OK"
OFFICE="$ROOT/source/tcz-office"
while IFS= read -r n; do
  [ -n "$n" ] || continue
  cp -f "$OFFICE/$n" "$MNT/cde/optional/$n"
  if [[ -f "$OFFICE/${n}.dep" ]]; then
    cp -f "$OFFICE/${n}.dep" "$MNT/cde/optional/${n}.dep"
  fi
  if [[ -s "$OFFICE/${n}.md5.txt" ]]; then
    cp -f "$OFFICE/${n}.md5.txt" "$MNT/cde/optional/${n}.md5.txt"
  fi
done <<'OFFICE_LIST'
Xorg-7.7-lib.tcz
abiword.tcz
adwaita-icon-theme.tcz
aspell-en.tcz
aspell.tcz
enchant2.tcz
gnumeric.tcz
goffice.tcz
gtk-update-icon-cache.tcz
gtk3.tcz
hunspell-en-us.tcz
hunspell.tcz
icu70.tcz
libFS.tcz
libXaw.tcz
libXfont2.tcz
libXpm.tcz
libXres.tcz
libXvmc.tcz
libgcrypt.tcz
libgpg-error.tcz
libgsf.tcz
libical3.tcz
libwmf.tcz
libxkbfile.tcz
libxslt.tcz
mpfr.tcz
raptor.tcz
rasqal.tcz
redland.tcz
wv.tcz
yajl2.tcz
OFFICE_LIST
if grep -qx 'abiword.tcz' "$MNT/cde/onboot.lst" || grep -qx 'gnumeric.tcz' "$MNT/cde/onboot.lst"; then
  echo "FAIL office packages load at boot"
  exit 1
fi
test -f "$MNT/cde/optional/abiword.tcz"
test -f "$MNT/cde/optional/gnumeric.tcz"
test -s "$MNT/cde/optional/abiword.tcz.dep"
test -s "$MNT/cde/optional/gnumeric.tcz.dep"
echo "OFFICE_TCZ_OK"
WB=/tmp/minios-v261-wba
rm -rf "$WB"
unsquashfs -d "$WB" "$MNT/cde/optional/wbar.tcz" >/tmp/minios-v261-wbar-unsquash.log
[[ -f "$WB/usr/local/bin/wbar.sh.real" ]] || { echo "FAIL wbar real starter missing"; exit 1; }
install -m 0755 "$SCRIPTS/wbar-wait.sh" "$WB/usr/local/bin/wbar.sh"
sed -i 's/\r$//' "$WB/usr/local/bin/wbar.sh"
grep -q 'minios-dock.ready' "$WB/usr/local/bin/wbar.sh"
if grep -q 'st_wait' "$WB/usr/local/bin/wbar.sh"; then echo "FAIL bar still waits"; exit 1; fi
grep -q 'nohup wbar' "$WB/usr/local/bin/wbar.sh"
grep -q 'bar_n" -lt 45' "$WB/usr/local/bin/wbar.sh"
rm -f /tmp/minios-v261-wbar.tcz
( cd "$WB" && mksquashfs . /tmp/minios-v261-wbar.tcz -comp gzip -b 131072 -noappend -all-root >/tmp/minios-v261-wbar-mksquash.log )
cp -f /tmp/minios-v261-wbar.tcz "$MNT/cde/optional/wbar.tcz"
( cd "$MNT/cde/optional" && md5sum wbar.tcz > wbar.tcz.md5.txt )
echo "WBAR_WAITS"
cd "$EXT"
rm -rf "$EXT"/*
set +e
zcat "$MNT/boot/core.gz" | cpio -idm
echo "CPIO=$?"
set -e
md5of() { md5sum "$1" | awk '{print $1}'; }
[[ "$(md5of "$EXT/opt/minios/minios-player-bar")" == "$PLAYER_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-mplayer")" == "$MPLAYER_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-player-bar")" == "$TTBAR_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tiktok")" == "$GUI_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-release")" == "$REL_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-mplayer")" == "$TTMP_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-play")" == "$PLAY_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-search")" == "$SEARCH_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-guard")" == "$GUARD_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-now")" == "$NOW_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-yt-resolve")" == "$RESOLVE_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-yt-arm")" == "$ARM_MD5" ]]
grep -q 'tt_sync_clip=$(tt_make_clear_file' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'play saved file' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL base still plays the large file"; exit 1; fi
grep -q -- '-demuxer lavf' "$EXT/opt/minios/minios-mplayer"
if grep -q 'framedrop' "$EXT/opt/minios/minios-mplayer"; then echo "FAIL base youtube framedrop"; exit 1; fi
if grep -q 'drop_caches' "$EXT/opt/minios/minios-tt-release"; then echo "FAIL base release drops caches"; exit 1; fi
grep -q 'tt-release leave python and caches' "$EXT/opt/minios/minios-tt-release"
grep -q 'MiniOS-TC-v0.2.260' "$EXT/opt/minios/IMAGE-ID.txt"
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-thumbs")" == "$THUMB_MD5" ]]
grep -q 'tt_make_clear_file' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'minios-tt-mplaye$' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL base short helper"; exit 1; fi
grep -q '^MP=/opt/minios/minios-tt-mplayer$' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'best = inner' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'best = inne$' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL base size reader"; exit 1; fi
grep -q -- '-input file=/tmp/minios-tt-mp.cmd' "$EXT/opt/minios/minios-tt-mplayer"
if grep -q -- '-input file=/tmp/minios-tt-mp.fwd' "$EXT/opt/minios/minios-tt-mplayer"; then echo "FAIL base wrapper forward pipe"; exit 1; fi
if grep -q 'kill -STOP' "$EXT/opt/minios/minios-tt-mplayer"; then echo "FAIL base wrapper freezes the picture"; exit 1; fi
grep -q 'bg-scale' "$EXT/opt/minios/setbg.sh"
grep -a -q 'renable JavaScript' "$EXT/opt/minios/minios-page-guard.xpi"
grep -q 'c: exec /usr/local/bin/minios-games' "$EXT/opt/minios/swap-wbar.sh"
echo "BASE_OK"

install_one() {
  local name="$1"
  local src="$2"
  install -m 0755 "$src" "$EXT/opt/minios/$name"
  ln -sfn "/opt/minios/$name" "$EXT/usr/local/bin/$name"
  while IFS= read -r dest; do
    case "$dest" in
      "$EXT/opt/minios/$name") continue ;;
    esac
    if [[ -L "$dest" ]]; then
      continue
    fi
    install -m 0755 "$EXT/opt/minios/$name" "$dest"
  done < <(find "$EXT" -name "$name" -type f)
}
install_one minios-tt-player-bar "$SCRIPTS/minios-tt-player-bar"
install_one minios-tt-mplayer "$SCRIPTS/minios-tt-mplayer"
install_one minios-tiktok "$SCRIPTS/minios-tiktok"
install_one minios-tiktok-search "$SCRIPTS/minios-tiktok-search"
install_one minios-tiktok-play "$SCRIPTS/minios-tiktok-play"
install_one minios-streaming "$SCRIPTS/minios-streaming"
install_one minios-streaming-search "$SCRIPTS/minios-streaming-search"
install_one minios-streaming-play "$SCRIPTS/minios-streaming-play"
install_one minios-run "$SCRIPTS/minios-run"
install_one minios-reclaim "$SCRIPTS/minios-reclaim"
install_one swap-wbar.sh "$SCRIPTS/minios-swap-wbar"
install -m 0755 "$SCRIPTS/bootlocal.sh" "$EXT/opt/bootlocal.sh"
install -m 0644 "$ROOT/source/icons/streaming.png" "$EXT/opt/minios/streaming.png"
install -m 0644 "$ROOT/source/icons/document.png" "$EXT/opt/minios/document.png"
install -m 0644 "$ROOT/source/icons/spreadsheet.png" "$EXT/opt/minios/spreadsheet.png"
mkdir -p "$EXT/home/tc/.wmx/Applications" "$EXT/etc/skel/.wmx/Applications"
install -m 0755 "$SCRIPTS/wmx-Document" "$EXT/home/tc/.wmx/Applications/Document"
install -m 0755 "$SCRIPTS/wmx-Spreadsheet" "$EXT/home/tc/.wmx/Applications/Spreadsheet"
install -m 0755 "$SCRIPTS/wmx-Document" "$EXT/etc/skel/.wmx/Applications/Document"
install -m 0755 "$SCRIPTS/wmx-Spreadsheet" "$EXT/etc/skel/.wmx/Applications/Spreadsheet"
install -m 0755 "$SCRIPTS/bootsync.sh" "$EXT/opt/bootsync.sh"
printf '%s\n' '' 'Loading MiniOS' '' >"$EXT/etc/motd"
grep -q 'Loading MiniOS' "$EXT/opt/bootsync.sh"
grep -q 'Loading MiniOS' "$EXT/etc/motd"
if grep -q 'live desktop' "$EXT/opt/bootsync.sh" "$EXT/etc/motd"; then echo "FAIL old boot line"; exit 1; fi
grep -q 'keep_streaming_on_bar' "$EXT/opt/minios/swap-wbar.sh"
grep -q 'start_bar_after_icons' "$EXT/opt/minios/swap-wbar.sh"
if grep -q 'wbar.sh.real' "$EXT/opt/minios/swap-wbar.sh"; then echo "FAIL dock still restarts from the saved bar"; exit 1; fi
tt_quiet_ok "$EXT/opt/minios/minios-tt-mplayer"
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-play")" == "$NEW_PLAY_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-mplayer")" == "$NEW_TTMP_MD5" ]]
grep -q '^MP=/opt/minios/minios-tt-mplayer$' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'minios-tt-mplaye$' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed short helper"; exit 1; fi
if grep -q 'best = inne$' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed size reader"; exit 1; fi
test -x "$EXT/opt/minios/minios-tt-mplayer"
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-search")" == "$NEW_SEARCH_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tiktok")" == "$NEW_GUI_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-now")" == "$NOW_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tiktok-thumbs")" == "$NEW_THUMB_MD5" ]]
if grep -a -q 'Now playing' "$EXT/opt/minios/minios-tiktok"; then echo "FAIL installed search name"; exit 1; fi
grep -a -q 'Now playing - %s' "$EXT/opt/minios/minios-tt-player-bar"
[[ "$(md5of "$EXT/opt/minios/minios-tt-release")" == "$REL_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-mplayer")" == "$NEW_TTMP_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-guard")" == "$GUARD_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-player-bar")" == "$PLAYER_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-mplayer")" == "$MPLAYER_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-tt-player-bar")" == "$NEW_BAR_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-yt-resolve")" == "$RESOLVE_MD5" ]]
[[ "$(md5of "$EXT/opt/minios/minios-yt-arm")" == "$ARM_MD5" ]]
grep -q 'tt_take_play_lock || exit 1' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'kill -STOP' "$EXT/opt/minios/minios-tt-mplayer"; then echo "FAIL installed wrapper freezes the picture"; exit 1; fi
if grep -q -- '-input file=/tmp/minios-tt-mp.fwd' "$EXT/opt/minios/minios-tt-mplayer"; then echo "FAIL installed wrapper forward pipe"; exit 1; fi
grep -q -- '-input file=/tmp/minios-tt-mp.cmd' "$EXT/opt/minios/minios-tt-mplayer"
grep -q 'tt_keep_open_file' "$EXT/opt/minios/minios-tiktok-play"
grep -q '/tmp/minios-tt-open.mp4' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'rm -f /tmp/minios-ttplay.mp4' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'tt_sync_clip=$(tt_make_clear_file' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'tt_sync_clip=$(tt_make_sync_file' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed soft picture"; exit 1; fi
grep -q 'tt_sync_file_ok "$tt_sync_clip"' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'wait "$tt_clear_pid"' "$EXT/opt/minios/minios-tiktok-play"
grep -F -q 'if(gt(iw,ih),320,-2)' "$EXT/opt/minios/minios-tiktok-play"
grep -F -q 'flags=lanczos' "$EXT/opt/minios/minios-tiktok-play"
grep -F -q -- '-q:v 3' "$EXT/opt/minios/minios-tiktok-play"
grep -F -q -- '-bf 0' "$EXT/opt/minios/minios-tiktok-play"
if grep -q 'play saved file' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed large file"; exit 1; fi
if grep -q 'tt_small_clip=$(tt_make_light_file' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed unchecked file"; exit 1; fi
if grep -q 'This video is too heavy for this laptop.' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed refusal"; exit 1; fi
if grep -q -- '-fps' "$EXT/opt/minios/minios-tiktok-play"; then echo "FAIL installed fps flag"; exit 1; fi
if grep -q 'framedrop' "$EXT/opt/minios/minios-tt-mplayer"; then echo "FAIL installed wrapper framedrop"; exit 1; fi
grep -q 'tt_pick_budget "$TT_BUDGET_ARG"' "$EXT/opt/minios/minios-tiktok-search"
grep -a -q 'Search runs for %d seconds, then shows the list.' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'Now playing - %s' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'pausing_keep_force seek %d 2' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v231 vol' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v232 pause-bar' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v232 play-bar' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'Paused clip %d of %d' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v233 list-end' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v234 unmap' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'More videos' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'New search' "$EXT/opt/minios/minios-tt-player-bar"
grep -q 'tt_pull_more_unseen' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'MINIOS_TT_WATCHED' "$EXT/opt/minios/minios-tiktok-search"
grep -a -q 'MINIOS_TT_WATCHED' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'player-v233 ask' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'search-v234 hide' "$EXT/opt/minios/minios-tiktok"
grep -q 'tt_stop_hidden_work' "$EXT/opt/minios/minios-tiktok-play"
grep -q '/tmp/minios-tt-thumb-page.txt' "$EXT/opt/minios/minios-tiktok-thumbs"
if objdump -T "$EXT/opt/minios/minios-tt-player-bar" | grep -E 'GLIBC_2\.(3[5-9]|[4-9][0-9])'; then echo "FAIL installed bar needs a newer C library"; exit 1; fi
file "$EXT/opt/minios/minios-tt-player-bar" | grep -q '80386'
file "$EXT/opt/minios/minios-tiktok" | grep -q '80386'
grep -q 'Downloading media' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'Optimizing video playback' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'tt-high play' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'tt-high fell back' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'MINIOS_TT_HD' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'tt_use_high_file' "$EXT/opt/minios/minios-tiktok-play"
grep -q 'FAST_SHOW=16' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'more videos other sites' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'more videos saved list' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'more next page' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'FAIL lang' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'search lang=' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'same search continue' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'short fill round' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'fill until budget' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'fast skip lang' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'more until budget' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'more keep round' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'tt_more_keep_going' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'fast skip seen' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'tt_fill_short_list' "$EXT/opt/minios/minios-tiktok-search"
grep -a -q 'search-v245 go' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'search-v246 same' "$EXT/opt/minios/minios-tiktok"
grep -q 'rowcount)" -lt 12' "$EXT/opt/minios/minios-tiktok-search"
grep -q 'rowcount)" -lt 8' "$EXT/opt/minios/minios-tiktok-search"
grep -a -q 'search-v235 quality' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'search-v238 more' "$EXT/opt/minios/minios-tiktok"
grep -a -q 'search-v244 same' "$EXT/opt/minios/minios-tiktok"
if grep -a -q 'No further videos' "$EXT/opt/minios/minios-tiktok"; then echo 'FAIL old more-videos sentence'; exit 1; fi
grep -a -q 'player-v235 hold' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v237 load-line' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v240 stage' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v241 title' "$EXT/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v244 last' "$EXT/opt/minios/minios-tt-player-bar"
grep -q 'de1.api.radio-browser.info' "$EXT/opt/minios/minios-streaming-search"
grep -q 'Free-TV/IPTV' "$EXT/opt/minios/minios-streaming-search"
grep -a -q 'stream-v247 search' "$EXT/opt/minios/minios-streaming"
grep -q 'st_pick_low_hls' "$EXT/opt/minios/minios-streaming-play"
grep -q 'st_ready_player' "$EXT/opt/minios/minios-streaming-play"
grep -q 'st_ready_player || exit 1' "$EXT/opt/minios/minios-streaming-play"
grep -q 'lastcheckok' "$EXT/opt/minios/minios-streaming-search"
grep -a -q 'stream-v252 full' "$EXT/opt/minios/minios-streaming"
grep -a -q 'stream-v253 pop' "$EXT/opt/minios/minios-streaming"
grep -a -q 'stream-v254 kind' "$EXT/opt/minios/minios-streaming"
grep -a -q 'stream-v256 view' "$EXT/opt/minios/minios-streaming"
grep -a -q 'stream-v256 row' "$EXT/opt/minios/minios-streaming"
grep -q 'minios-st-geom' "$EXT/opt/minios/minios-streaming-play"
grep -q -- '-noborder' "$EXT/opt/minios/minios-streaming-play"
grep -q 'Video stations are included' "$EXT/opt/minios/minios-streaming-search"
grep -q 'video source' "$EXT/opt/minios/minios-streaming-search"
grep -q 'iptv-org.github.io' "$EXT/opt/minios/minios-streaming-search"
grep -q 'plutotv_us.m3u' "$EXT/opt/minios/minios-streaming-search"
grep -q 'More than one video list was searched' "$EXT/opt/minios/minios-streaming-search"
if grep -q -- '-wid' "$EXT/opt/minios/minios-streaming-play"; then echo 'FAIL packed picture uses -wid'; exit 1; fi
grep -q 'Each row is marked Video or Audio.' "$EXT/opt/minios/minios-streaming-search"
grep -q 'minios-st-cmd' "$EXT/opt/minios/minios-streaming-play"
grep -q 'minios-streaming' "$EXT/opt/minios/swap-wbar.sh"
test -f "$EXT/opt/minios/streaming.png"
test -f "$EXT/opt/minios/document.png"
test -f "$EXT/opt/minios/spreadsheet.png"
test -x "$EXT/home/tc/.wmx/Applications/Document"
test -x "$EXT/home/tc/.wmx/Applications/Spreadsheet"
grep -q 'add_office_icons' "$EXT/opt/minios/swap-wbar.sh"
grep -q '^keep_streaming_on_bar$' "$EXT/opt/minios/swap-wbar.sh"
grep -q 'Document is loading' "$EXT/opt/minios/minios-run"
grep -q 'setbg.sh' "$EXT/opt/minios/swap-wbar.sh"
grep -q 't: Document' "$EXT/opt/minios/swap-wbar.sh"
grep -q 'i" -lt 40' "$EXT/opt/bootlocal.sh"
grep -q 'bar_n" -lt 45' "$SCRIPTS/wbar-wait.sh"
grep -q '/opt/minios/swap-wbar.sh' "$SCRIPTS/wbar-wait.sh"
grep -q 'setbg.sh' "$SCRIPTS/minios-swap-wbar"
grep -q 't: Document' "$SCRIPTS/minios-swap-wbar"
file "$EXT/opt/minios/minios-streaming" | grep -q '80386'
echo "SCRIPT_INSTALLED"

BG_STAMP=/tmp/minios-v261-bg.jpg
python3 "$SCRIPTS/stamp-image-id.py" "$EXT/opt/minios/mini-os-bg.jpg" "$BG_STAMP" "$CAPTION"
file "$BG_STAMP" | grep -q '1792x1008'
while IFS= read -r -d '' wp; do
  install -m 0644 "$BG_STAMP" "$wp"
done < <(find "$EXT" -type f -name 'mini-os-bg.jpg' -print0)
echo "$CAPTION" >"$EXT/opt/minios/IMAGE-ID.txt"
echo "$VER" >"$EXT/opt/minios/VERSION"
if [[ -f "$EXT/etc/os-release" ]]; then
  sed -i 's/^VERSION_ID=.*/VERSION_ID="0.2.261"/' "$EXT/etc/os-release" || true
  sed -i 's/^VERSION=.*/VERSION="0.2.261"/' "$EXT/etc/os-release" || true
fi
if grep -q 'Linux Kernel MiniOS' "$EXT/etc/init.d/tc-config" 2>/dev/null; then
  sed -i 's/Linux Kernel MiniOS ${YELLOW}0\.2\.260/Linux Kernel MiniOS ${YELLOW}0.2.261/' \
    "$EXT/etc/init.d/tc-config" || true
  grep -q '0.2.261' "$EXT/etc/init.d/tc-config"
fi
echo "STAMP_OK"
cd "$EXT"
find . | cpio -o -H newc 2>/dev/null | gzip -9 > /tmp/minios-v261-core.gz
gzip -t /tmp/minios-v261-core.gz
CHK=/tmp/minios-v261-chk
rm -rf "$CHK"; mkdir -p "$CHK"
( cd "$CHK" && zcat /tmp/minios-v261-core.gz | cpio -idm >/dev/null )
test "$(md5of "$CHK/opt/minios/minios-tiktok-play")" = "$NEW_PLAY_MD5"
grep -q '^MP=/opt/minios/minios-tt-mplayer$' "$CHK/opt/minios/minios-tiktok-play"
if grep -q 'minios-tt-mplaye$' "$CHK/opt/minios/minios-tiktok-play"; then echo "FAIL core short helper"; exit 1; fi
test "$(md5of "$CHK/opt/minios/minios-tiktok-search")" = "$NEW_SEARCH_MD5"
test "$(md5of "$CHK/opt/minios/minios-tiktok")" = "$NEW_GUI_MD5"
test "$(md5of "$CHK/opt/minios/minios-tt-now")" = "$NEW_NOW_MD5"
test "$(md5of "$CHK/opt/minios/minios-tt-release")" = "$REL_MD5"
test "$(md5of "$CHK/opt/minios/minios-tt-mplayer")" = "$NEW_TTMP_MD5"
test "$(md5of "$CHK/opt/minios/minios-tt-guard")" = "$GUARD_MD5"
test "$(md5of "$CHK/opt/minios/minios-player-bar")" = "$PLAYER_MD5"
test "$(md5of "$CHK/opt/minios/minios-mplayer")" = "$MPLAYER_MD5"
test "$(md5of "$CHK/opt/minios/minios-tt-player-bar")" = "$NEW_BAR_MD5"
test "$(md5of "$CHK/opt/minios/minios-tiktok-thumbs")" = "$NEW_THUMB_MD5"
grep -a -q 'Now playing - %s' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'pausing_keep_force seek %d 2' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v231 vol' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v232 pause-bar' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v232 play-bar' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'Paused clip %d of %d' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v233 list-end' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v234 unmap' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'More videos' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'New search' "$CHK/opt/minios/minios-tt-player-bar"
grep -q 'tt_pull_more_unseen' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'MINIOS_TT_WATCHED' "$CHK/opt/minios/minios-tiktok-search"
grep -a -q 'MINIOS_TT_WATCHED' "$CHK/opt/minios/minios-tiktok"
grep -a -q 'player-v233 ask' "$CHK/opt/minios/minios-tiktok"
grep -a -q 'search-v234 hide' "$CHK/opt/minios/minios-tiktok"
if objdump -T "$CHK/opt/minios/minios-tt-player-bar" | grep -E 'GLIBC_2\.(3[5-9]|[4-9][0-9])'; then echo "FAIL core bar needs a newer C library"; exit 1; fi
grep -q '/tmp/minios-tt-thumb-page.txt' "$CHK/opt/minios/minios-tiktok-thumbs"
grep -q 'tt_stop_hidden_work' "$CHK/opt/minios/minios-tiktok-play"
test "$(md5of "$CHK/opt/minios/minios-yt-resolve")" = "$RESOLVE_MD5"
test "$(md5of "$CHK/opt/minios/minios-yt-arm")" = "$ARM_MD5"
grep -q 'tt_sync_clip=$(tt_make_clear_file' "$CHK/opt/minios/minios-tiktok-play"
if grep -q 'tt_sync_clip=$(tt_make_sync_file' "$CHK/opt/minios/minios-tiktok-play"; then echo "FAIL core soft picture"; exit 1; fi
grep -q 'wait "$tt_clear_pid"' "$CHK/opt/minios/minios-tiktok-play"
grep -F -q 'if(gt(iw,ih),320,-2)' "$CHK/opt/minios/minios-tiktok-play"
grep -F -q -- '-bf 0' "$CHK/opt/minios/minios-tiktok-play"
if grep -q 'play saved file' "$CHK/opt/minios/minios-tiktok-play"; then echo "FAIL core large file"; exit 1; fi
grep -q 'tt_pick_budget' "$CHK/opt/minios/minios-tiktok-search"
grep -q '133/160' "$CHK/opt/minios/minios-yt-resolve"
grep -q 'yt_paint_first_clock' "$CHK/opt/minios/minios-yt-arm"
if grep -q 'drop_caches' "$CHK/opt/minios/minios-tt-release"; then echo "FAIL core drop_caches"; exit 1; fi
if grep -q 'This video is too heavy for this laptop.' "$CHK/opt/minios/minios-tiktok-play"; then echo "FAIL core refusal"; exit 1; fi
grep -q 'reset_zram 12' "$CHK/opt/minios/minios-reclaim"
grep -q -- '-demuxer lavf' "$CHK/opt/minios/minios-mplayer"
if grep -q 'framedrop' "$CHK/opt/minios/minios-mplayer"; then echo "FAIL core youtube framedrop"; exit 1; fi
grep -q 'bg-scale' "$CHK/opt/minios/setbg.sh"
grep -q '0.2.261' "$CHK/etc/init.d/tc-config"
tt_quiet_ok "$CHK/opt/minios/minios-tt-mplayer"
grep -q 'tt_load_picture_libs' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'libavdevice4' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'tt_take_play_lock' "$CHK/opt/minios/minios-tiktok-play"
if grep -q 'kill -STOP' "$CHK/opt/minios/minios-tt-mplayer"; then echo "FAIL core wrapper freezes the picture"; exit 1; fi
if grep -q -- '-input file=/tmp/minios-tt-mp.fwd' "$CHK/opt/minios/minios-tt-mplayer"; then echo "FAIL core wrapper forward pipe"; exit 1; fi
grep -q -- '-input file=/tmp/minios-tt-mp.cmd' "$CHK/opt/minios/minios-tt-mplayer"
grep -q 'tt_keep_open_file' "$CHK/opt/minios/minios-tiktok-play"
grep -q '/tmp/minios-tt-open.mp4' "$CHK/opt/minios/minios-tiktok-play"
grep -a -q 'MiniOS TikTok Player - Now playing' "$CHK/opt/minios/minios-tt-now"
if grep -a -q 'Now playing' "$CHK/opt/minios/minios-tiktok"; then echo "FAIL core search name"; exit 1; fi
grep -a -q 'Search runs for %d seconds, then shows the list.' "$CHK/opt/minios/minios-tiktok"
grep -q 'Downloading media' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'Optimizing video playback' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'tt-high play' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'tt-high fell back' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'MINIOS_TT_HD' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'tt_use_high_file' "$CHK/opt/minios/minios-tiktok-play"
grep -q 'FAST_SHOW=16' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'more videos other sites' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'more videos saved list' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'more next page' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'FAIL lang' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'search lang=' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'same search continue' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'short fill round' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'fill until budget' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'fast skip lang' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'more until budget' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'more keep round' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'tt_more_keep_going' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'fast skip seen' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'tt_fill_short_list' "$CHK/opt/minios/minios-tiktok-search"
grep -a -q 'search-v245 go' "$CHK/opt/minios/minios-tiktok"
grep -a -q 'search-v246 same' "$CHK/opt/minios/minios-tiktok"
grep -q 'rowcount)" -lt 12' "$CHK/opt/minios/minios-tiktok-search"
grep -q 'rowcount)" -lt 8' "$CHK/opt/minios/minios-tiktok-search"
grep -a -q 'search-v235 quality' "$CHK/opt/minios/minios-tiktok"
grep -a -q 'search-v238 more' "$CHK/opt/minios/minios-tiktok"
grep -a -q 'search-v244 same' "$CHK/opt/minios/minios-tiktok"
if grep -a -q 'No further videos' "$CHK/opt/minios/minios-tiktok"; then echo 'FAIL old more-videos sentence'; exit 1; fi
grep -a -q 'player-v235 hold' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v237 load-line' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v240 stage' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v241 title' "$CHK/opt/minios/minios-tt-player-bar"
grep -a -q 'player-v244 last' "$CHK/opt/minios/minios-tt-player-bar"
grep -q 'de1.api.radio-browser.info' "$CHK/opt/minios/minios-streaming-search"
grep -q 'Free-TV/IPTV' "$CHK/opt/minios/minios-streaming-search"
grep -a -q 'stream-v247 search' "$CHK/opt/minios/minios-streaming"
grep -q 'st_pick_low_hls' "$CHK/opt/minios/minios-streaming-play"
grep -a -q 'stream-v247 play' "$CHK/opt/minios/minios-streaming"
grep -a -q 'stream-v252 full' "$CHK/opt/minios/minios-streaming"
grep -a -q 'stream-v253 pop' "$CHK/opt/minios/minios-streaming"
grep -a -q 'stream-v254 kind' "$CHK/opt/minios/minios-streaming"
grep -a -q 'stream-v256 view' "$CHK/opt/minios/minios-streaming"
grep -a -q 'stream-v256 row' "$CHK/opt/minios/minios-streaming"
grep -q 'minios-st-geom' "$CHK/opt/minios/minios-streaming-play"
grep -q -- '-noborder' "$CHK/opt/minios/minios-streaming-play"
grep -q 'Video stations are included' "$CHK/opt/minios/minios-streaming-search"
grep -q 'video source' "$CHK/opt/minios/minios-streaming-search"
grep -q 'iptv-org.github.io' "$CHK/opt/minios/minios-streaming-search"
grep -q 'plutotv_us.m3u' "$CHK/opt/minios/minios-streaming-search"
grep -q 'More than one video list was searched' "$CHK/opt/minios/minios-streaming-search"
if grep -q -- '-wid' "$CHK/opt/minios/minios-streaming-play"; then echo 'FAIL checked picture uses -wid'; exit 1; fi
grep -q 'Each row is marked Video or Audio.' "$CHK/opt/minios/minios-streaming-search"
grep -q 'minios-st-cmd' "$CHK/opt/minios/minios-streaming-play"
grep -q 'st_ready_player' "$CHK/opt/minios/minios-streaming-play"
grep -q 'lastcheckok' "$CHK/opt/minios/minios-streaming-search"
if grep -q 'st_wait' "$CHK/usr/local/bin/wbar.sh" "$CHK/opt/minios/wbar-wait.sh" 2>/dev/null; then echo "FAIL packed bar still waits"; exit 1; fi
grep -q 'scope streaming' "$CHK/opt/minios/minios-reclaim"
grep -q 'minios-streaming' "$CHK/opt/minios/swap-wbar.sh"
grep -q 'keep_streaming_on_bar' "$CHK/opt/minios/swap-wbar.sh"
grep -q 'Loading MiniOS' "$CHK/opt/bootsync.sh"
grep -q 'Loading MiniOS' "$CHK/etc/motd"
if grep -q 'live desktop' "$CHK/opt/bootsync.sh" "$CHK/etc/motd"; then echo "FAIL packed old boot line"; exit 1; fi
test -f "$CHK/opt/minios/streaming.png"
test -f "$CHK/opt/minios/document.png"
test -f "$CHK/opt/minios/spreadsheet.png"
test -x "$CHK/home/tc/.wmx/Applications/Document"
test -x "$CHK/home/tc/.wmx/Applications/Spreadsheet"
grep -q 'add_office_icons' "$CHK/opt/minios/swap-wbar.sh"
grep -q 'setbg.sh' "$CHK/opt/minios/swap-wbar.sh"
grep -q 'i" -lt 40' "$CHK/opt/bootlocal.sh"
echo "INIT_OK"
cp -f /tmp/minios-v261-core.gz "$MNT/boot/core.gz"
echo "$CAPTION" >"$MNT/IMAGE-ID.txt"
printf '%s\n' "$CAPTION" "The bar starter builds the icon list, then draws the menu once. Volume MINIOS-2261." >"$MNT/README-USB.txt"
while IFS= read -r -d '' wp; do
  install -m 0644 "$BG_STAMP" "$wp"
  file "$wp" | grep -q '1792x1008'
done < <(find "$MNT" -type f -name 'mini-os-bg.jpg' -print0)
USED2=$(df --output=pcent "$MNT" | tail -1 | tr -dc '0-9')
echo "FAT_USED_AFTER=$USED2"
if [[ "${USED2:-0}" -gt 85 ]]; then echo "FAIL FAT ${USED2}%"; exit 1; fi
sync
umount "$MNT"
trap - EXIT
bash "$SCRIPTS/set-img-volname.sh" "$WORK" "$CAPTION"
cp -f "$WORK" "$IMG_OUT"
cp -f "$WORK" "$BACKUP_IMG"
rm -f "$WORK"
MNT2=/tmp/minios-v261-ro
rm -rf "$MNT2"
mkdir -p "$MNT2"
mount -o loop,offset=1048576,ro "$IMG_OUT" "$MNT2"
grep -q 'MiniOS-TC-v0.2.261' "$MNT2/IMAGE-ID.txt"
grep -q 'MINIOS-2261' "$MNT2/README-USB.txt"
for need in libavdevice4 libavfilter4 libavresample4 SDL2 libv4l2; do
  [[ -f "$MNT2/cde/optional/${need}.tcz" ]]
done
echo "TCZ_RO_OK"
USED3=$(df --output=pcent "$MNT2" | tail -1 | tr -dc '0-9')
echo "FAT_USED_RO=$USED3"
umount "$MNT2"
if [[ "${USED3:-0}" -gt 85 ]]; then echo "FAIL FAT ${USED3}%"; exit 1; fi
cmp -s "$IMG_OUT" "$BACKUP_IMG"
echo "COPIES_MATCH"
echo "BAKE_NEXT_OK $IMG_OUT"
ls -lh "$IMG_OUT" "$BACKUP_IMG"
