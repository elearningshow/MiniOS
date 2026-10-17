# Binaries copied from MiniOS-TC-v0.2.260-bar.img

These three files are the players on the stick. Do not replace them with the same names from `source/scripts/` or from `source/restore-106/`.

| File | MD5 | What it is |
| --- | --- | --- |
| `minios-player-bar` | `4f98713e736ac7f892d7ceccb053606e` | YouTube player bar. 32-bit ELF. |
| `minios-mplayer` | `a2ecd453d68056ba8ed0b50cc1108082` | YouTube mplayer wrapper. Shell script. The same bytes are in `source/restore-106/minios-mplayer`. |
| `minios-tt-player-bar` | `1ac1e77d242a1798292ef7df9badc4cc` | TikTok player bar. 32-bit ELF. The same bytes are in `source/scripts/minios-tt-player-bar`. |

Files that look similar and must not be copied onto the stick:

| File | MD5 | Why it stays off the stick |
| --- | --- | --- |
| `source/scripts/minios-player-bar` | `fb898f735c318d573802662423370d6e` | A different YouTube player file. Do not recompile it onto the stick. |
| `source/scripts/minios-mplayer` | `e9722ff51e6287d5426912d375ce3ffd` | A later wrapper. Shipping it broke playback. |
| `source/restore-106/minios-player-bar` | `31e3231563e09f6d3eaf8abcec59dc1d` | An older saved player. Not the file on this image. |
