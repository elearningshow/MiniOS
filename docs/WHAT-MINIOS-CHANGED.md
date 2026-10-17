# What MiniOS changed in Tiny Core 16.2

Tiny Core 16.2 32-bit is the system underneath MiniOS. MiniOS keeps Tiny Core’s boot, its package files (`.tcz`), its window manager (flwm), its dock (wbar), and its desktop start (`/etc/skel/.xsession` from Xlibs). The MiniOS programs are added inside `core.gz` under `/opt/minios/` and are started from the dock and from `/usr/local/etc/X.d`.

This is not a fork of https://github.com/tinycorelinux . That group is several small repositories. The 16.2 kernel and extensions are published at http://tinycorelinux.net/16.x/x86/ . MiniOS does not fork Tianocore. Tianocore is a different project.

## The machine

The stick is for a Gateway MA3. One 32-bit core. About 441 MB of working memory. A 512 MB swap file created before the desktop starts. About 93 MB of compressed RAM (zram). No hardware video decoder. Live USB only. The internal disk is never an install target.

## Boot

- The boot text says `Loading MiniOS` instead of the stock Tiny Core “live desktop” line and the add-on list.
- The corner caption is `MiniOS-TC-v0.2.260`.
- The FAT volume name is `MINIOS-2260`. A FAT name cannot contain dots and can be at most 11 characters. The name is `MINIOS-` plus the minor number and the patch number.
- Wireless firmware for the Broadcom 4318 (b43, `ucode5.fw`) is loaded from the boot script.
- The swap file is created before X. Creating it after the desktop had started removed the bottom bar.
- The desktop picture is 1792 by 1008 and is painted with `feh --bg-scale`. `feh --bg-fill` is not used.

## Bottom bar

Tiny Core draws the bar from `/usr/local/tce.icons`. Each button is three lines: `i:` the picture, `t:` the name you see when the pointer rests on it, `c:` the command. An empty row drops a button.

On this image the bar is drawn once. `wbar.sh` (the file `source/scripts/wbar-wait.sh`) runs the icon script, waits until the list is marked finished, starts `wbar` one time, and exits. The icon script writes Streaming, Document, and Spreadsheet into that list and does not stop the bar and start it again.

An earlier image, v0.2.251, could draw the bar, then stop it and draw it again after Streaming was added. That second start is not in this image’s icon script.

## Programs MiniOS added

| Program | Role | Separate from |
| --- | --- | --- |
| YouTube search and player | Search YouTube and play the chosen video. | TikTok and Streaming. Own player, own downloader, own `/tmp` names (`minios-mp`, `minios-yt`). |
| TikTok search and player | Search TikTok and play the chosen video. The seconds button default is 30. | YouTube and Streaming. Own `/tmp` names (`minios-tt`). |
| Streaming | Keyword search of free radio and free picture stations. Audio controls sit on the selected row. A picture station opens a Streaming window. | YouTube and TikTok. Own `/tmp` names (`minios-st-*`). Closing it does not kill every `mplayer`. A live station has no time slider. |
| Nightly | Firefox ESR with pictures on and JavaScript off until the user turns it on. | The players. |
| Text Browser | One text-only window. | Nightly. |
| Media Player | Local files. | The network players. |
| Games | Tetris and LBreakout2, downloaded on first use. | The players. |
| AI | A chat window that uses the network. | The optional local language-model test, which is not in this image. |
| Document | AbiWord, loaded on click from `abiword.tcz`. | Boot. It is not in the boot list. |
| Spreadsheet | Gnumeric, loaded on click from `gnumeric.tcz`. | Boot. It is not in the boot list. |
| Refresh RAM | Releases memory after a heavy program. It does not delete the swap file. | The swap-create step. |
| Wifi | The Tiny Core wireless tool, kept on the bar. | |

Document and Spreadsheet also appear under Applications. The mouse-over names are Document and Spreadsheet.

## Players that stay as saved binaries

Three files on the stick are saved binaries. They are in `source/usb-binaries/` with these MD5 values:

- YouTube player bar `4f98713e736ac7f892d7ceccb053606e`
- YouTube mplayer wrapper `a2ecd453d68056ba8ed0b50cc1108082`
- TikTok player bar `1ac1e77d242a1798292ef7df9badc4cc`

`source/scripts/minios-player-bar` (MD5 `fb898f735c318d573802662423370d6e`) is not the stick binary. `source/scripts/minios-mplayer` (MD5 `e9722ff51e6287d5426912d375ce3ffd`) is not the stick wrapper. `source/restore-106/minios-player-bar` (MD5 `31e3231563e09f6d3eaf8abcec59dc1d`) is an older saved file and is not the stick binary. The restore-106 mplayer wrapper does match the stick.

The picture path does not add `-framedrop`, `-hardframedrop`, `-correct-pts`, `-nocorrect-pts`, `-autosync`, `-fps`, `-mc`, or `-speed`. It does not drop caches. The TikTok picture stays the real-size 320-long-side file unless High is turned on. High is for a faster computer. On the Gateway, leave High off. The Streaming picture uses `-geometry`, `-noborder`, `-zoom`, and `-xy 480`. It does not use `-wid`.

## Packages

AbiWord, Gnumeric, and the libraries they need are in `source/tcz-office/` and already on the image under `cde/optional/`. They are not listed in the boot package list. The first click runs `tce-load` for that one program. The FAT use of this image was 83 percent when it was built. A later bake must stay at or under 85 percent.

## What a fork edits

Edit `source/scripts/` and bake with `bake/build-next-image.sh`. The script copies this image and replaces the programs inside it. Read `docs/RULES.md` before the edit. If a note and a file in `source/scripts/` disagree, the file wins.
