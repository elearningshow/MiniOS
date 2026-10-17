# Rules for a change

Read this before editing `source/scripts/`. The Gateway MA3 has one 32-bit core and about 441 MB of working memory. A change that needs a second core, 64-bit code, a hardware decoder, or a large copy in memory will not run there.

1. Stay on one program’s files and that program’s `/tmp` names. YouTube uses `minios-mp` and `minios-yt`. TikTok uses `minios-tt`. Streaming uses `minios-st` and `minios-streaming`.
2. Add a new function and new variables. Copy the lines you need. Leave a working function as it is. A change that rewrote a shared YouTube function broke YouTube.
3. A new function is finished only when the same edit starts it, calls it, waits for it, and clears the pid file it owns.
4. Do not replace `source/usb-binaries/minios-player-bar`, `source/usb-binaries/minios-mplayer`, or `source/usb-binaries/minios-tt-player-bar`.
5. Do not ship `source/scripts/minios-mplayer`. Do not compile `source/scripts/minios-player-bar` onto the stick.
6. Do not add `-framedrop`, `-hardframedrop`, `-correct-pts`, `-nocorrect-pts`, `-autosync`, `-fps`, `-mc`, or `-speed` to a player. Do not drop caches.
7. The wallpaper stays 1792 by 1008, painted with `feh --bg-scale`.
8. FAT use stays at or under 85 percent. Do not overwrite `MiniOS-TC-v0.2.260-bar.img`. The next bake writes a new file under `out/`.
9. Do not put AbiWord or Gnumeric in the boot package list. They load when their button is clicked.
10. Leave the TikTok seconds button at 30. Do not tell a user to switch it to 60.
11. A live Streaming station does not get a seek. Closing Streaming does not kill every `mplayer`.
12. The sentences “This video did not start.” and “Could not load the video.” stay for a real failure. Changing the sentence is not a playback fix.
13. `docs/FUNCTION-MAP.md` describes the calls. Some of its lists were written for an older image. The script in `source/scripts/` wins when they disagree.
