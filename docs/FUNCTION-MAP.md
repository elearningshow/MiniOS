# MiniOS function map

Written: 2026-09-22. Live USB is Tiny Core 16.2, 32-bit. The call lists below were written for `MiniOS-TC-v0.2.155-play-close`. The image to flash now is in `sessions/CURRENT.md`. If this map and a live file disagree, the live file wins.

This is the map of how the pieces call each other. Read it before changing a script. The safe edit is the one that stays on one app's files and one app's `/tmp` names.

## 0. Before any revision

The laptop is a Gateway MA3. Mobile AMD Sempron 3300+ at 2.0 GHz, one core, 32-bit i686. Working RAM is 441 MB. The swap file is 512 MB. ZRAM is 93 MB. There is no hardware video decoder. A change that needs a second core, 64-bit code, a hardware decoder, or a large copy in RAM will not run here. Filling the 441 MB spills into the swap file and the one core becomes too slow to play video.

Do this before editing:

1. Read this file from the start through the section for that app. The sections are boot, dock, `minios-run`, YouTube, the YouTube player, TikTok, close and Refresh RAM, `/tmp` names, bake, the file list, and "If you change this, also check that."
2. Open the live file under `poc/tinycore/scripts/` and read the function you will change plus the functions that call it. Repo-root `scripts/` is the old antiX tree and is not on the USB.
3. Stay on that app's files and that app's `/tmp` names. YouTube uses `minios-mp` and `minios-yt`. TikTok uses `minios-tt`. When you insert new code, the feature that already works must keep working. Add new variables and a new function, and copy the lines you need into that function. Leave the working function as it is. A media-type check can share one function when the YouTube lines stay the ones that already work and the TikTok lines are their own branch. Rewriting that shared function is what broke YouTube. A new function is finished only when the same edit starts it, calls it, and closes it: the process is waited on, and the flag, the pid, and the temporary file it owns are cleared. Do not leave the call or the cleanup for a later patch.
4. Copying code is accepted until the end of the project. Security review and shortening come after the features work.
5. Do not recompile `minios-player-bar` or `minios-tt-player-bar`. The USB runs the saved player binaries, not those source files.
6. Name the new image id before baking. Do not overwrite an older `.img`.
7. Read `sessions/SESSION-START.md` and run its triple check before the reply, and again before the bake. A requirement with no sentence in the reply, or no line in the diff, is not done.

## 1. Which tree is the product

| Tree | What it is | On the USB? |
|---|---|---|
| `poc/tinycore/scripts/` | The live MiniOS programs. Bakes copy these into `core.gz` under `/opt/minios/`. | Yes |
| `poc/tinycore/fltube-src/` | YouTube search GUI. Compiled to `fltube` and installed as `/opt/minios/bin/fltube`. | Yes, as the binary |
| `poc/tinycore/restore-106/minios-mplayer` | YouTube mplayer wrapper that is actually shipped. | Yes |
| `poc/tinycore/scripts/minios-player-bar.cxx` | Source for the player. **Not the binary on the USB.** | No. The USB binary is the v0.2.106 ELF, MD5 `4f98713e`, with one same-length patch |
| `poc/tinycore/scripts/minios-mplayer` | A later wrapper. **Do not ship it.** It writes `/tmp/minios-mp.reload` during a download and kills play | No |
| `scripts/` at the repo root, `config/`, `desktop/` | Older antiX desktop. Not the Gateway image | No |
| `wbar.tcz` on the FAT partition | Tiny Core dock. `wbar_setup.sh` creates `xwbar.lst`. Not a file in this repo | Yes, inside the tcz |

A change in `scripts/` at the repo root does not change the laptop. A change in `minios-player-bar.cxx` does not change the laptop until someone recompiles and replaces the ELF. Do not do that.

## 2. Boot, in order

```
Tiny Core init
  /etc/motd                         "MiniOS live desktop" plus the app names
  bootsync.sh
      minios-fix-tcedir             /etc/sysconfig/tcedir must be a symlink to the USB cde/
      minios-xvesa-install          Xvesa -nozap before the desktop
      bootlocal.sh in the background
      minios-sound-up boot-chime    play mini-mini.mp3 once, then unmount that tree
  bootlocal.sh
      minios-log-usb boot-early     Windows can read the log even if X fails
      minios-swap-early             swap file BEFORE X. Creating it later killed wbar
      minios-xvesa-nozap
      fix-home.sh
      setbg.sh once X is up
      b43 firmware + modprobe b43
  wbar.tcz before X
      wbar_setup.sh                 builds /usr/local/tce.icons from dot.wbar
  startx / .xsession from Xlibs
      wbar.sh in the background     waits for /tmp/minios-dock.ready
      /usr/local/etc/X.d/zzzz-minios-swap
          swap-wbar.sh              writes the dock, then touches that ready file
          swap-activate.sh          after 2 seconds
      wbar.sh.stock                 starts wbar after the ready file exists
```

`minios-swap-early` and `swap-wbar.sh` are different jobs. The first makes the swap file before X. The second only fills the dock. Do not create the swap file from `swap-wbar.sh`. Do not `pkill -x wbar` there. A restart removed the menu and left no icons. The desktop sources `/usr/local/etc/X.d/`, not `/etc/X.d/`. v0.2.162 wrote the icon script into the folder that does not run, and the bar had already started.

`minios-xsession-stock` must not be installed into `core.gz`. Never `echo 'exec flwm' > /home/tc/.xsession`.

## 3. Dock

`swap-wbar.sh` edits `/usr/local/tce.icons` (rows `i:` icon, `t:` title, `c:` command). Empty rows drop icons. v0.2.255 starts the bottom bar once, after Streaming is already on the list. It does not draw the bar and then draw it again. v0.2.256 puts the audio controls on the selected row and opens a player window for a video station. A search returns video stations as well as audio stations. v0.2.257 searches the next free picture list when the first list has fewer than six channels whose names match the word. v0.2.258 adds Document and Spreadsheet. Those icons load AbiWord and Gnumeric when they are clicked. The same names are under Applications in the desktop menu. v0.2.259 puts Document and Spreadsheet in the same icon block as YouTube and TikTok. The bar starts once after that list is finished, and the wallpaper is painted when the screen is up.

| Function | What it does |
|---|---|
| `append_icon` | Adds one dock row if that command is not already there |
| `place_tiktok_beside_youtube` | Order is YouTube, TikTok, AI, Wi-Fi, Refresh RAM, Exit |

Every dock command is `minios-run <name> -- <program>`. The desktop files in `poc/tinycore/scripts/*.desktop` are the same commands.

| Icon | Command |
|---|---|
| YouTube | `minios-run youtube -- minios-youtube` |
| TikTok | `minios-run tiktok -- minios-tiktok` |
| AI | `minios-run ai -- minios-ai-chat` immediately left of Wi-Fi. Picture is `/opt/minios/ai.png` |
| Nightly | `minios-run nightly -- minios-firefox` |
| Text Browser | `minios-run minios-text-browser -- minios-text-browser` |
| Quick Search | `minios-run minios-quick-search -- minios-quick-search` |
| Media Player | `minios-run player -- minios-player` |
| Refresh RAM | `minios-refresh-ram` directly. Same job as Ctrl+Alt+Delete |
| Create Swap | Not added by the current `swap-wbar.sh`. That script drops music and Create Swap rows. If a row is already in `tce.icons`, `minios-run` must `exec` it. Waiting and then `pkill` closed the window |

`minios-refresh-desktop` and `minios-reset-desktop` rebuild icon rows. They must not add a second Media Player row.

## 4. minios-run, the front door

`minios-run` is the only dock launcher.

| Piece | Job |
|---|---|
| alert text | "YouTube is loading", "TikTok is loading", and so on, via `minios-loading` |
| `wait_if_reclaiming` | A new click waits while `/tmp/minios-reclaiming` exists, so reclaim does not kill the new app |
| lock `/tmp/minios-run-<name>.lock` | One copy. A second click shows the notice and does not start another |
| `exec` for Create Swap, Refresh RAM, wbar | Those must not be waited on. Waiting and then `pkill` closed Create Swap |
| `wait` on the app | When the window exits, cleanup runs |
| `run_close_reclaim` | Shows "Please wait." / "Freeing memory." then `sudo -n minios-reclaim auto <app>` |
| `show_ram_wait` | The notice. Prefers `minios-cad --notice` |

Closing YouTube or TikTok only shows Freeing memory if this script is the one on the USB. An older `minios-run` without `run_close_reclaim` was on the stick before v0.2.155. That is why the notice disappeared.

## 5. YouTube, click by click

YouTube must not call TikTok programs, and must not use `/tmp/minios-tt-*`.

```
dock YouTube
  minios-run youtube
    minios-youtube
      minios-fix-tcedir
      minios-sound-up          background, must not block FLTube
      exec fltube
        main
          fit_search_to_screen     window inset 40px so flwm's left buttons stay on screen
          layout_fltube_chrome
            place_result_row       thumbnail stays 95x80, title uses the rest
        doSearch
          update_video_info
        preview_video_cb           user clicked a result
          YtDlp_Helper::write_playlist("/tmp/minios-yt-playlist", url)
          show_fltube_shade
            start_minios_player
              if player already running: write /tmp/minios-mp.reload and return
              else start minios-player-bar --youtube
  player ELF (not the cxx)
    load_playlist                  first line is the index, then url<TAB>title<TAB>seconds
    start_pl_item(pl_cur)          automatic on open
      execl minios-shorts-play URL
        minios-yt-dlp -g URL       MINIOS_QUEUE_PLAY=1 so it does not start a second bar
          minios-yt-resolve
            ytdlp_download_2026    mount Python 3.11, yt-dlp 2026, video then audio then captions
            else ytdlp_download_youtube_staged   yt-dlp 2025, usually fails (SABR)
            else piped / invidious / ytdlp_download
        exec minios-mplayer LOCALFILE
          -loop 1, attach /tmp/minios-ytplay.audio and captions
          write /tmp/minios-mp.done
          do not write /tmp/minios-mp.reload for a local file
```

FLTube functions that matter:

| Function | File | Called by | Must keep doing |
|---|---|---|---|
| `main` | `FLTube.cxx` | process start | `fit_search_to_screen` after `show` |
| `fit_search_to_screen` | `FLTube.cxx` | `main` | Leave 40px on the left and 56px for the dock |
| `layout_fltube_chrome` | `FLTube.cxx` | window `resize` | Call `place_result_row` for each of 4 rows |
| `place_result_row` | `FLTube.cxx` | layout | Thumbnail 95x80. Do not let group resize scale it over the title |
| `doSearch` | `FLTube.cxx` | Go search | YouTube search only. Not TikTok |
| `update_video_info` | `FLTube.cxx` | after results | Layout first, then load a 95px thumbnail |
| `preview_video_cb` | `FLTube.cxx` | thumbnail click | Write the playlist, then `show_fltube_shade`. Do not also fork `shorts-play` |
| `start_minios_player` | `FLTube.cxx` | `show_fltube_shade` | Runs `minios-yt-arm`. Does not use `pgrep -f` (that matches its own shell and skips starting the player) |
| `write_playlist` | `ytdlp_helper.cxx` | `preview_video_cb` | Index is the clicked video id, not always 0 |
| `stream` | `ytdlp_helper.cxx` | old FLTube path | Not the MiniOS click path. Do not send search here |
| `exitApp` | `FLTube.cxx` | window close | Process exits. `minios-run` then reclaims |

`minios-yt-resolve` functions, in the order `main` uses them:

| Function | Job |
|---|---|
| `video_id` | 11-character id from `v=`, `/shorts/`, or `youtu.be/` |
| `is_tiktok` | Refuse. Tell the log to use `minios-tiktok-play` |
| `ensure_py311` | Run `minios-py311-setup`. Never `tar` Python into RAM |
| `ytdlp_download_2026` | The download that works. Format 133/160, then audio 140, then `fetch_captions` |
| `run_ytd_2025` | Percent parser for the 2025 client. Used by the staged fallback |
| `ytdlp_download_youtube_staged` | Video, audio, captions with yt-dlp 2025. First try plain, second try tv/ios/web_safari |
| `fetch_captions` | English auto-subs. Uses 2026 if Python 3.11 is mounted |
| `main` fallbacks after 2026 | `ytdlp_download_youtube_staged`, then `try_piped`, `try_invidious`, then `ytdlp_download`. These are not inside `ytdlp_download_2026` |
| `write_progress` | `/tmp/minios-dl.progress`: line 1 percent, line 2 label |
| `take_download` | Move the saved file to `/tmp/minios-ytplay.mp4` |
| `play_env` | SSL certs and `PYTHONHTTPSVERIFY=0` |
| `ytdlp_download_tiktok` | Returns immediately. Do not grow this |

`minios-py311-setup` finds `/mnt/*/minios-py311.sqfs` and calls `minios-py311-mount` through `sudo -n`. `minios-py311-mount` is the only command allowed to mount or unmount that squashfs. `minios-refresh-sudoers` must list it.

`minios-yt-play` is the old shell downloader (video, audio, captions, then mplayer). It is still on disk. **shorts-play must not exec it.** Both scripts can write the exact line "The video file was not downloaded." Seeing that line does not prove the click went through `minios-yt-play`. Current `minios-shorts-play` calls `minios-yt-dlp -g` and writes that line itself when resolve returns nothing. The working call is `minios-yt-dlp -g`.

`minios-yt-arm` is the YouTube-only helper a search click runs. It writes `/tmp/minios-player-mode` as `youtube` and `/tmp/minios-media-source` as `YouTube`. It starts `minios-player-bar --youtube` when `/tmp/minios-player-bar.pid` is not a live process. If the downloader is not running after the window is up, it writes `/tmp/minios-mp.reload` so the player calls the same function as the playlist button. If that still did not start a download, it runs `minios-shorts-play` on the playlist URL. It quits YouTube through `/tmp/minios-mp.cmd` only. It does not `pkill` mplayer and it does not touch TikTok files. Do not go back to `pgrep -f` to decide whether the player is up.

`minios-yt-focus` runs when YouTube opens so the Term/URL caret is ready. It is a small helper, not FLTube.

`minios-youtube-fail` is the "YouTube did not open" notice if `fltube` is missing.

## 6. The YouTube player that is actually running

The USB file `/opt/minios/minios-player-bar` is an ELF. MD5 `4f98713e736ac7f892d7ceccb053606e`. It was built from the v0.2.106 source and then patched: the bytes `loop 1\n\0osd 1\n` became `loop -1\nosd 1\n`. `patch-player-loop.py` does that and nothing else.

The cxx in the repo has the same shape, so this is what the binary does. Editing the cxx does not change the binary.

| Function | What the running player does |
|---|---|
| `load_playlist` | Reads `/tmp/minios-yt-playlist`. Line 1 is the index. Later lines are `url`, tab, `title`, tab, seconds. At most 40 rows (`MAXPL`) |
| `start_pl_item` | Clears the fail file, stops the old download, forks `minios-shorts-play` with that URL |
| `read_progress` | Second line of `/tmp/minios-dl.progress` is the status text ("Downloading video") |
| `read_pid_files` | If `/tmp/minios-mp.fail` exists, the window says "Could not load the video" |
| `apply_loop_to_player` | Sends slave `loop -1` (the patch). Do not recompile a `loop -1 1` over this |
| startup | If a playlist exists and nothing is playing, call `start_pl_item` once |
| reload | If `/tmp/minios-mp.reload` appears, load the playlist again and `start_pl_item` if mplayer is not running |
| `request_quit` | Quit file, kill download, hide windows |
| Playlist button | `open_playlist` / a row calls `start_pl_item` for that row. This is why a playlist pick can play after the first click failed |

`minios-mplayer` (restore-106, MD5 `a2ecd453d68056ba8ed0b50cc1108082`):

- CLI `-loop 1` before the filename means play once. `-loop 0` and `-loop -1` are forever.
- For `/tmp/minios-ytplay.*` it sets local mode and does **not** write `/tmp/minios-mp.reload`.
- Attaches `/tmp/minios-ytplay.audio` and the caption file.
- Writes `/tmp/minios-mp.done` after playback starts. The bar will not advance to the next clip without that file.

Do not replace this wrapper with `poc/tinycore/scripts/minios-mplayer`.

## 7. TikTok, click by click

TikTok must not call `minios-yt-play`, `minios-yt-resolve`, `minios-player-bar`, or `minios-mplayer`. It must not write `/tmp/minios-mp.*` or `/tmp/minios-yt-*`.

```
dock TikTok
  minios-run tiktok
    minios-tiktok                 the X11 window from minios-tiktok.c
      do_search
        fork minios-tiktok-search QUERY
          Bing, Yahoo, then DuckDuckGo POST pages
          echo STATUS lines the window paints while it waits
          keep only @user/video/digits; resolve short links first and skip failures
        thumbs_for_page
          minios-tiktok-thumbs
      play_item
        write /tmp/minios-tt-playlist
        write /tmp/minios-tt-dl.progress
        start minios-tt-player-bar if it is not up
        fork minios-tiktok-play URL
          yt-dlp on the canonical post, TikWM only if that misses, then minios-tt-mplayer
            mplayer without forcing lavf, -autosync 30
            write /tmp/minios-tt-mp.done with the play generation
      finish_app on close
        minios-tt-cleanup
  minios-run sees the window exit
    run_close_reclaim tiktok
      minios-reclaim auto tiktok
        kill_tiktok_only
        reset_zram
        drop_caches
```

`minios-tiktok.c` functions:

| Function | Job |
|---|---|
| `main` | Open the window. `x_error` logs to `/tmp/minios-tiktok.log` and does not exit. Focus only on `MapNotify` |
| `paint` / `layout` / `text_at` / `draw_thumb` | Draw the search window |
| `do_search` | One search. `STATUS` lines update the status text. Result cap `MAXR` is 120 |
| `thumbs_for_page` / `start_thumbs` | Page of pictures. Does not block play |
| `play_item` | Write the TikTok playlist and start the TikTok player and downloader |
| `release_search_ram` | Kill search and thumb processes and delete search temps. Does not kill a playing clip |
| `finish_app` | Close path. Runs `minios-tt-cleanup` |

`minios-tt-player-bar.cxx` is the TikTok player source. It is a clone. It may be compiled to `minios-tt-player-bar`. It must keep using `/tmp/minios-tt-*` only. Do not copy a TikTok edit back into `minios-player-bar.cxx` and ship that as YouTube.

`minios-tiktok-play` accepts only `https://www.tiktok.com/@user/video/digits`. It tries yt-dlp on that post first (`--no-playlist --merge-output-format mp4`), then TikWM. The player opens only after HTTP status, content type, final URL, file size, and a video stream all check out. `minios-tt-mplayer` plays that file. `-demuxer lavf` made some clips run ahead of the sound. v0.2.155 does not force lavf and sets `-autosync 30 -mc 0 -correct-pts`.

## 8. Close, Ctrl+Alt+Delete, Refresh RAM

```
Ctrl+Alt+Delete
  /opt/minios/minios-cad-init     inittab ctrlaltdel
    minios-cad                    the notice "Closing applications and freeing memory."
      minios-reclaim full

Refresh RAM dock icon
  minios-refresh-ram              same reclaim as CAD. minios-run execs it and does not lock

App window closed
  minios-run run_close_reclaim <name>
    notice "Please wait." / "Freeing memory."
    minios-reclaim auto <name>
```

`minios-reclaim` functions:

| Function | Job |
|---|---|
| `user_app_running` | If auto was called with no app name and something is still up, only reap. Do not swapoff |
| `kill_user_apps` | CAD/full: Firefox, FLTube, both players, both downloaders |
| `kill_tiktok_only` | TikTok processes and `/tmp/minios-tt-*` only. Does not kill YouTube |
| `kill_text_browser_only` | links |
| youtube scope | Kill YouTube player and downloader. Unmount `/tmp/minios-py311` before removing it. Do not kill TikTok |
| `reset_zram` | swapoff, mkswap, swapon each zram device |
| `reset_disk_if_high` | Disk swap only if it is busy. Never delete the swap file |
| `drop_caches_root` | `echo 3 > /proc/sys/vm/drop_caches` |
| `wipe_tmpfs_caches` | Big caches under `/home/tc` and `/tmp` |

`minios-yt-cleanup` is YouTube-only: kill shorts-play, yt-resolve, the YouTube player, and YouTube temps. It unmounts the Python squashfs. It must not `pkill` every `python3.9` (Nightly needs it) and must not touch TikTok.

`minios-tt-cleanup` is TikTok-only. It must not remove `/tmp/minios-mp.*` or `/tmp/minios-yt-playlist`.

`minios-after-close` is what the player forks on Quit. It is separate from `minios-run`.

`minios-appkit` loads and unloads tcz groups per app (`do_load`, `do_unload`, `kill_kit_procs`, `wipe_kit_tmp`). Reclaim calls `unload` for the scope. A kit unload must not remove a tcz another running app still needs (`other_kit_needs`, `is_keep_tcz`).

## 9. /tmp names, do not cross

YouTube:

| Path | Who writes | Who reads |
|---|---|---|
| `/tmp/minios-yt-playlist` | `write_playlist` | player `load_playlist` |
| `/tmp/minios-dl.progress` | `write_progress`, shorts-play | player `read_progress` |
| `/tmp/minios-mp.fail` | resolve / shorts-play on failure | player shows "Could not load the video" |
| `/tmp/minios-mp.reload` | FLTube when the player is already open | player reloads the playlist and starts that clip |
| `/tmp/minios-mp.done` | `minios-mplayer` after playback starts | player advances Auto / Loop |
| `/tmp/minios-mp.quit` | Quit, reclaim | shorts-play and mplayer refuse to start |
| `/tmp/minios-mp.cmd` `/tmp/minios-mp.out` `/tmp/minios-mp.pid` | mplayer slave | player `send_cmd` |
| `/tmp/minios-ytplay.mp4` | resolve `take_download` | mplayer |
| `/tmp/minios-ytplay.audio` | resolve after the audio file | mplayer `-audiofile` |
| `/tmp/minios-ytplay.sub` and `*.srt` | `fetch_captions` | mplayer `-sub` |
| `/tmp/minios-play-gen` | player `bump_gen` | shorts-play `MINIOS_PLAY_GEN` |
| `/tmp/minios-ytplay.lock` | `minios-yt-dlp -g` | one download at a time |
| `/tmp/minios-py311` | mount of `minios-py311.sqfs` | yt-dlp 2026. Unmount before delete |

TikTok:

| Path | Who writes | Who reads |
|---|---|---|
| `/tmp/minios-tt-playlist` | `play_item` | TikTok player |
| `/tmp/minios-tt-dl.progress` | `minios-tiktok-play` | TikTok window status and TikTok player |
| `/tmp/minios-tt-mp.done` | `minios-tt-mplayer` if the generation matches and playback started | TikTok bar Loop / Auto |
| `/tmp/minios-tt-mp.quit` `/tmp/minios-tt-mp.cmd` `/tmp/minios-tt-mp.pid` | TikTok player and mplayer | TikTok only |
| `/tmp/minios-tt-rows.txt` | `minios-tiktok-search` | `do_search` |
| `/tmp/minios-tiktok.ready` | TikTok GUI | `minios-run` knows the window is up |

`/tmp/minios-yt-tiktok` is a mode flag. YouTube search must `unlink` it. TikTok may set it. Do not use it as a shared downloader switch.

AI:

| Path | Who writes | Who reads |
|---|---|---|
| `/tmp/minios-ai.ready` | chat window after it opens | `minios-run` stops the loading notice |
| `/tmp/minios-ai.pid` | chat window | reclaim kills that pid only |
| `/tmp/minios-ai-hist.json` | `minios-ai-ask` | the next turn. Removed when the window opens |
| `/tmp/minios-ai-out.txt` | `minios-ai-ask` | the chat window shows this reply |
| `/tmp/minios-dock.ready` | `swap-wbar.sh` after the icon list | `wbar.sh` then starts the bar |

## 10. Other programs

| Program | Entry | Notes |
|---|---|---|
| `minios-firefox` | Nightly dock | Do not let reclaim or a player kill it unless the scope is nightly or full |
| `minios-text-browser` | links in aterm | |
| `minios-quick-search` | stays in its own aterm. Must not open Text Browser as a second window | |
| `minios-player` | dock Media Player | Local files. Must not be blocked because a YouTube bar is up (`minios-run` returns "not running" for the player name on purpose) |
| `minios-mp3` / `minios-music` | MP3 | Do not attach a YouTube `-audiofile` to an MP3 |
| `minios-wifi` | wireless | `minios-sound-up` and reclaim must not restart Wi-Fi or drop caches during boot chime |
| `minios-ai-chat` | AI dock | Chat window. User lines on the right, replies on the left. Does not run a local model |
| `minios-ai-ask` | started by the chat window | Posts the last few messages to `https://text.pollinations.ai/openai` |
| `minios-loading` | the small "please wait" box | `minios-loading stop` closes it |
| `minios-cad` / `minios-cad.c` | the CAD notice window | `--notice` is the two-line Freeing memory box |
| `minios-resmon` / `minios-resmon-x11.c` | RAM meter | Stay above other windows. Do not call `XSetInputFocus` |
| `minios-log-usb` | copy logs to the FAT `minios-logs` folder | |
| `minios-drop-caches` | small sudo helper | listed in sudoers |
| `minios-create-swap` | Create Swap icon | 512 MB file, aterm, not `minios-run` wait |
| `earlyoom-i686` | optional low-RAM killer | |

## 11. Bake

A new image is `poc/tinycore/build-bake-ap01-vN.sh`, run as root (`wsl -u root`). It copies the previous `.img`, unpacks `core.gz` with cpio as root, replaces only the files that change, packs with `find . | cpio -o -H newc | gzip -9`, stamps the wallpaper with `stamp-image-id.py`, and sets the volume with `set-img-volname.sh`.

Rules that have already broken a flash:

- Do not rewrite cpio members with a homemade script. v0.2.149 did that and the dock and YouTube download steps were gone.
- FAT used must stay at or under 85 percent. Volume is `MINIOS-21NN` (11 characters).
- Wallpaper caption is `MiniOS-TC-v0.2.N` only.
- Do not replace `minios-player-bar` or `minios-mplayer` unless the task is an explicit restore of those two files as a pair.
- `/dev/null` must still be in `core.gz`. No `/home/tc/.xsession`.
- Copy the finished `.img` to `backup/` and `poc/tinycore/`. Never overwrite an older image.

## 12. Every file in poc/tinycore/scripts

Live programs (installed under `/opt/minios` by a bake):

| File | Use |
|---|---|
| `bootsync.sh` | Before X. tcedir, Xvesa, chime |
| `bootlocal.sh` | Swap file, wallpaper, b43 |
| `zzzz-minios-swap` | X.d entry that runs the dock script |
| `zzzzz-minios-cad` | CAD hook |
| `zzzzz-minios-resmon` | RAM meter hook |
| `minios-run` | Dock launcher and close-reclaim notice |
| `minios-swap-wbar` | Dock rows |
| `minios-swap-early` | Swap file before X |
| `minios-swap-activate` | Turn swap on after the dock |
| `minios-swap` `minios-swap-tune` | Swap helpers. Do not run them during desktop start |
| `minios-youtube` | Opens FLTube |
| `minios-yt-arm` | Search click: start the player, then the same download as the playlist button |
| `minios-yt-focus` `minios-yt-focus.c` | Puts the caret in Term/URL |
| `minios-yt-dlp` | Search uses yt-dlp 2025. `-g` calls `minios-yt-resolve` |
| `minios-yt-resolve` | Download video, audio, captions |
| `minios-yt-play` | Old shell downloader. Do not call from shorts-play |
| `minios-yt-queue` | Do not route YouTube through this |
| `minios-yt-cleanup` | YouTube temps and player |
| `minios-shorts-play` | The name the YouTube player always execs |
| `minios-mplayer` | Later wrapper. Do not ship. Ship `restore-106/minios-mplayer` |
| `minios-player-bar` | Shipped ELF if present in the tree. The cxx is not this file |
| `minios-player-bar.cxx` | Player source. Do not recompile over the patched ELF |
| `minios-player-bar.c` | Older C source. Not the USB binary |
| `minios-player` | Dock Media Player for local files |
| `minios-player-keys` | Key helper |
| `minios-tiktok` | Compiled search window |
| `minios-tiktok.c` | Source of that window |
| `minios-tiktok-search` | Bing, Yahoo, DuckDuckGo, tikwm resolve |
| `minios-tiktok-thumbs` | Row pictures |
| `minios-tiktok-play` | canonical video URL, yt-dlp first, TikWM fallback, then TikTok mplayer |
| `minios-tt-mplayer` | TikTok playback and `/tmp/minios-tt-mp.done` |
| `minios-tt-player-bar` | TikTok player binary |
| `minios-tt-player-bar.cxx` | TikTok player source. Keep it a clone |
| `minios-tt-cleanup` | TikTok temps |
| `minios-py311-setup` `minios-py311-mount` | Mount Python 3.11 from the squashfs |
| `minios-python3` | Tiny Core python helper for search |
| `minios-reclaim` | RAM, zram, caches |
| `minios-drop-caches` | sudo helper |
| `minios-cad` `minios-cad.c` `minios-cad-init` | Ctrl+Alt+Delete notice |
| `minios-refresh-ram` | Dock icon for the same reclaim |
| `minios-refresh-desktop` `minios-reset-desktop` | Rebuild dock rows |
| `minios-loading` | Please-wait box |
| `minios-after-close` | Player Quit hook |
| `minios-appkit` `minios-apps` | tcz load/unload per app |
| `minios-firefox` | Nightly |
| `minios-text-browser` `minios-quick-search` `minios-ddg` | Text search |
| `minios-mp3` `minios-mp3.c` `minios-music` | MP3 |
| `minios-wifi` | Broadcom 4318 / b43 |
| `minios-sound-up` | Mount mplayer/ffmpeg/alsa tczs, boot chime |
| `minios-fix-tcedir` `minios-fix-home` `minios-persist` | USB tcedir symlink and home |
| `minios-load-tcz` | Load one extension from the USB |
| `minios-xvesa-install` `minios-xvesa-nozap` `minios-xvesa-wrap` `minios-xvesa-wrap.c` `minios-patch-xvesa-nozap` | Xvesa without Ctrl+Alt+Backspace killing X |
| `minios-startx` `minios-start-gui` `minios-xsession-stock` | X start. Do not install the stock xsession into core |
| `minios-log-usb` `minios-startup-log` `minios-hwinfo` `minios-check` | Logs |
| `minios-internal` | Guard so nothing installs onto the IDE disk |
| `minios-create-swap` | Create Swap icon |
| `minios-refresh-sudoers` | The only `sudo -n` commands. Add a helper here or `sudo` will ask for a password |
| `minios-resmon` `minios-resmon-x11` `minios-resmon-x11.c` `minios-rsmon` | RAM meter |
| `minios-show-image` | Picture helper |
| `minios-earlyoom` `earlyoom-i686` | Optional early OOM killer |
| `minios-baseline-check` `minios-ap06-check` `minios-check-autostart` | Checks used while baking, not on the laptop menu |
| `minios-browser` `minios-browser-measure` `minios-diag-dillo` | Old browser experiments |
| `minios-youtube-fail` `minios-youtube-fail-open` | FLTube failed to open |
| `minios-setuphome-quote` | Home setup quote helper |
| `stamp-image-id.py` | Paint `MiniOS-TC-v0.2.N` on the wallpaper |
| `set-img-volname.sh` | FAT label `MINIOS-21NN` |
| `patch-player-loop.py` | Same-length ELF patch only |
| `fltube.conf` `fltube-userdata.txt` | FLTube defaults |
| `*.desktop` | Dock and menu command lines |
| `firefox-user.js` `minios-view-image.xpi` | Nightly settings |

Host-only (never copied as a running Gateway program): `fetch-*.sh`, `prepare-b43-firmware.sh`, `make-*-icon.py`, `compile` helpers live next to the bake scripts in `poc/tinycore/`.

## 13. Before changing anything

Follow section 0 first. The laptop limits and the read-the-live-file steps are there.

1. Name the app. YouTube or TikTok or dock or boot. Do not edit both players for one app.
2. If the file is `minios-player-bar.cxx` or `scripts/minios-mplayer`, stop. The USB does not run those files.
3. If the change adds a `sudo` command, add that exact path to `minios-refresh-sudoers` or it will fail on the laptop.
4. If the change deletes `/tmp/minios-py311`, unmount it first.
5. New behavior needs a new image id. Do not overwrite `backup/MiniOS-TC-v0.2.*.img`.
6. Flash target and the do-not-flash list are in `sessions/CURRENT.md` and `sessions/PROJECT-IDS.md`.

## 14. What a close actually kills

`minios-run` waits for the dock program. On exit it sets `/tmp/minios-reclaiming`. YouTube and TikTok search then call `minios-gui-close`, which shows "Please wait." / "Freeing memory.", logs free RAM, zram used, and swap-file used, and runs `minios-reclaim auto`. Other dock apps use `run_close_reclaim`, which logs the same three numbers. Player-only close does not reclaim; that memory is freed when the search window closes. These names `exec` and never wait: `wbar`, `exit`, `reboot`, `shutdown`, Refresh RAM, Create Swap, `aterm`.

Before reclaim, `minios-run` itself already does this:

| Name that just exited | Extra kills in `minios-run`, before `minios-reclaim` |
|---|---|
| youtube | `pkill` YouTube `minios-player-bar`, then `pkill -x mplayer`. That second kill is every mplayer, including a TikTok clip that is playing at the same time |
| tiktok | search, thumbs, `minios-tiktok-play`. Removes `/tmp/minios-tt-dl` only when the YouTube player bar is not running |
| text-browser, quick-search | `pkill -x links` |
| player | `app_up` always returns "not running", so a second Media Player click is allowed. Close still reclaims the player name |

`minios-reclaim auto <scope>` then calls `minios-appkit unload` for that scope, then the scope case, then `reset_zram`, `reset_disk_if_high`, `drop_caches_root`.

| Scope | Processes | Files |
|---|---|---|
| `youtube` | shorts-play, yt-resolve, YouTube player-bar, every `mplayer` | unmount `/tmp/minios-py311`, then `rm -rf` only if it is not still a mount. Also `/tmp/minios-ca-bundle.crt` and `wipe_tmpfs_caches` |
| `tiktok` | `kill_tiktok_only`: search, thumbs, play, `minios-tiktok`, `minios-tt-player-bar`, `minios-tt-mplayer` | TikTok `/tmp` names. Does not call the YouTube kill list |
| `text-browser` or `quick-search` | `links`, both browser scripts | |
| `player` | appkit player kill (YouTube bar, mplayer, yt-resolve) | appkit also removes YouTube playlist files if `fltube` is not running |
| `nightly` | Firefox and its content processes | Mozilla caches |
| empty or `full` | `kill_user_apps`: Firefox, FLTube, both players, both downloaders, yt-dlp | `unload-all` |

`minios-appkit` hazards, already in the source. Do not "clean these up" without a Gateway test, and do not copy them into the other app:

- `kill_kit_procs tiktok` kills `minios-player-bar` and `mplayer` when `fltube` is not running. Closing TikTok while the YouTube search window is already closed can stop the YouTube player.
- `wipe_kit_tmp youtube` does `rm -rf /tmp/minios-py311` with no unmount. Reclaim's youtube case unmounts after appkit returns. A future edit must unmount before that `rm`. Deleting the unmount in reclaim leaves a live mount.
- `wipe_kit_tmp tiktok` removes `/tmp/minios-yt-tiktok`, and removes `/tmp/minios-py311` when `fltube` is down.
- `kit_tczs` for youtube and tiktok both list `python3.9`, `ffmpeg4`, and `mplayer-cli`. `other_kit_needs` must stay. Unloading one kit while the other is `.active` must not unmount a shared squashfs.
- `is_keep_tcz` never unmounts X, flwm's libraries, wbar, aterm, feh, or b43.

`sudo -n` is not open. `minios-refresh-sudoers` allows only: `minios-drop-caches`, `minios-reclaim`, `minios-reclaim auto`, `minios-reclaim full`, `minios-fix-tcedir`, `minios-sound-up`, `minios-sound-up libs-only`, `minios-log-usb`, `minios-py311-mount`. Each path is listed twice (`/opt/minios` and `/usr/local/bin`). A new root helper fails on the laptop until both lines are added. Do not replace the image sudoers file. Wi-Fi lines may exist only on the stick.

## 15. YouTube player: source functions versus the binary

`poc/tinycore/scripts/minios-player-bar.cxx` is the shape of the player. The USB binary is the v0.2.106 ELF, MD5 `4f98713e736ac7f892d7ceccb053606e`, with one same-length patch. Compiling this cxx and shipping it replaces that ELF. Do not.

The cxx `start_pl_item` (around line 1585) forks `minios-tiktok-play` when the URL contains `tiktok.com`, and otherwise forks `minios-shorts-play`. That TikTok fork is source drift. It is not a reason to recompile. TikTok play starts from `minios-tiktok.c` `play_item`, not from this file.

`apply_loop_to_player` sends `loop -1` then `osd 1`. The shipped patch is the 14-byte change of `loop 1` to `loop -1`. `loop -1 1` does not fit.

Source functions, grouped by what they touch:

| Group | Functions | Breaks if changed |
|---|---|---|
| Start a clip | `start_pl_item`, `bump_gen`, `stop_download`, `start_local_file`, `cache_play` | Clears `FAILF` and `DONEF`, writes progress, forks shorts-play. The playlist button and the automatic first clip both use this |
| Playlist file | `load_playlist`, `save_pl_index`, `drop_pl_item`, `clear_playlist`, `load_local_playlist`, `save_local_playlist`, `keep_only_local_items` | Line 1 of `/tmp/minios-yt-playlist` is the index. `MAXPL` is 40 |
| Slave control | `send_cmd`, `read_answers`, `apply_loop_to_player`, `toggle_pause`, `set_vol`, `seek_pct`, `do_cc`, `request_cc`, `clear_subs` | Talks to `/tmp/minios-mp.cmd` and `.out` |
| End of clip | `take_clip_done`, `finish_current_clip`, `read_pid_files`, `mplayer_alive` | Needs `/tmp/minios-mp.done`. `FAILF` sets `load_failed` ("Could not load the video") |
| Reload | main loop watches `RELOADF` `/tmp/minios-mp.reload` | Writing this file during a download kills the download. FLTube may write it only when the bar is already up |
| Quit | `request_quit`, `clear_youtube_media` | Quit file, kill mplayer, delete YouTube temps. `clear_youtube_media` also `pkill -x mplayer` |
| Chrome | `layout_bar`, `refresh_ui`, `show_bar`, `hide_bar`, `apply_card`, `toggle_max`, `toggle_min`, `restack_play`, `pin_stage`, `place_shield`, `grab_keys`, `raise_resmon` | The v0.2.129 break stopped per-tick `grab_keys` and title raise, then shipped the rebuilt ELF. Do not repeat that |
| Buttons | `cb_tog`, `cb_prev`, `cb_next`, `cb_auto`, `cb_loop`, `cb_list`, `cb_add`, `cb_cc`, `cb_quit`, `open_playlist`, `cb_list_pick` | Playlist row calls `start_pl_item` |
| Local files | `open_audio_folder`, `add_scan_dir`, `add_to_playlist`, `open_file_browser`, `is_local_url` | Dock Media Player uses this path. Do not attach a YouTube audio sidecar |

`minios-player-bar.c` is an older C player. It is not the USB binary.

`poc/tinycore/scripts/minios-mplayer` is a later wrapper. It writes `/tmp/minios-mp.reload`. Do not ship it.

`poc/tinycore/restore-106/minios-mplayer` is the wrapper on the USB (MD5 `a2ecd453d68056ba8ed0b50cc1108082`):

- `LOCALYT=1` when the argument matches `/tmp/minios-ytplay.`. That case does not write `/tmp/minios-mp.reload`.
- A non-local YouTube URL does write reload (line 209). A search click must hand it a local file, not a watch URL.
- Sidecar audio from `/tmp/minios-ytplay.audio` or `/tmp/minios-ytplay.a.m4a` / `.a.aac`, and captions, only when `YTPLAY=1` and the target is not `http`.
- CLI is `-loop 1` (play once) unless `MINIOS_LOOP=1`, which becomes `-loop 0` (forever).
- It forces `-demuxer lavf`. That stays on the YouTube wrapper. TikTok removed lavf for a different reason. Do not copy one mplayer line onto the other.
- Writes `/tmp/minios-mp.pid` and, after playback starts, `/tmp/minios-mp.done`.

## 16. Download functions, in call order

`minios-shorts-play` (no functions, straight through):

1. Record `/tmp/minios-dl.pid` and `MINIOS_PLAY_GEN` from `/tmp/minios-play-gen`. Set `MINIOS_QUEUE_PLAY=1`.
2. If the URL contains `tiktok.com`, `exec minios-tiktok-play`. That is a live cross-call. YouTube playlists must not contain TikTok URLs. Do not delete the guard, and do not send YouTube URLs through it.
3. Clear `/tmp/minios-mp.fail`. Refuse to start if `/tmp/minios-mp.quit` exists and the player bar is still up.
4. `minios-yt-dlp -g URL`. On failure write "The video file was not downloaded." and `/tmp/minios-mp.fail`.
5. `exec minios-mplayer` on the printed path. No reload file.

`minios-yt-dlp` functions: `usb_log`, `show_loading`, `bar_up`, `start_player_bar`.

- Search (no `-g`) stays on yt-dlp 2025. Do not send search through yt-dlp 2026.
- `-g` / `--get-url` calls `minios-yt-resolve`. With `MINIOS_QUEUE_PLAY=1` it does not start a second player bar. Without that variable it may start `minios-player-bar --youtube`.
- Lock directory `/tmp/minios-ytplay.lock`. A stale lock used to skip resolve and flash "Video is loading".
- `-o -` is the old FLTube pipe. It cats a local file or downloads a URL. The MiniOS click does not use it. `minios-youtube` sets `ENABLE_ALTERNATIVE_STREAM_METHOD = false` and `STREAM_PLAYER_PATH = minios-mplayer` so FLTube's own `stream()` is not the click path.

`minios-yt-resolve` `main`:

1. `--captions` calls `fetch_captions` only.
2. `is_tiktok` exits 1. Do not grow `ytdlp_download_tiktok`.
3. `video_id`, else exit 1.
4. `write_progress(0, "Downloading video")`.
5. `read_cache`. A local file bigger than 1024 bytes is printed and the process exits. A stale cache is why a second click can skip the download. FLTube `start_minios_player` unlinks `/tmp/minios-yt-resolve.cache` before starting.
6. `ytdlp_download_2026` (mount Python 3.11, format 133/160, then audio 140, then `fetch_captions`).
7. `ytdlp_download_youtube_staged` (python3.9 and yt-dlp 2025). YouTube SABR usually returns no formats here.
8. If `recent_fail` is false: `try_piped`, then `try_invidious`, then `ytdlp_download`.
9. `write_fail` and exit 1. That creates `/tmp/minios-mp.fail`.

`ensure_py311` runs `minios-py311-setup`, which loop-mounts `/mnt/*/minios-py311.sqfs`. It must not `tar -xzf` Python into RAM.

`mux_split_tracks` exists and uses ffmpeg. The 2026 path must not depend on it. ffmpeg mux has hung this machine.

`minios-youtube` before `exec fltube`: fix tcedir, start `minios-sound-up` in the background (a foreground sound-up made "YouTube did not open" while FLTube was fine), clear `/tmp/minios-mp.quit`, `.fail`, and `.reload`. It does not touch `/tmp/minios-tt-*`.

## 17. TikTok functions

Checked 2026-10-14 against the live TikTok player bar, `minios-tiktok.c`, `minios-tiktok-search`, and `minios-tt-mplayer`. The call list below was written for v0.2.155. Where it disagrees with `poc/tinycore/scripts/`, the live file wins. v0.2.234 replaces the TikTok search window and the TikTok player bar. The search script and the TikTok mplayer script stay. v0.2.235 replaces the search window, the TikTok player bar, the search script, and the play script. The TikTok mplayer script stays. The first load line says Downloading media. The picture save says Optimizing video playback. Fit or High sits left of the seconds button. Fit still makes the 320-long-side mpeg4 picture. High keeps a larger H.264 file when that file can play, and that file does not go through tt_sync_file_ok. v0.2.235 could keep 24, and that longer fetch left too little time to check files. v0.2.236 replaces only the search script and keeps 16. v0.2.237 replaces only the TikTok player bar. v0.2.238 replaces the search script and the search window. v0.2.239 replaces only the search script. v0.2.240 replaces only the TikTok player bar. v0.2.241 replaces only the TikTok player bar. v0.2.242 replaces only the search script. v0.2.243 replaces only the search script. v0.2.244 replaces the search script, the search window, and the TikTok player bar. Go search with the same words continues links that were not shown. A different word clears that saved list. A failed last clip still offers More videos. v0.2.245 replaces the search script and the search window. The TikTok player bar stays. A first search with fewer than 8 videos checks one more page while time remains, at most twice. English drops a title that uses two common words from Spanish, French, German, or Portuguese. An empty continue says to press Go search! again. v0.2.246 replaces the search script and the search window. The TikTok player bar stays. More videos checks hits that were not shown, and a video already watched stays off that list. The same words continue even when the screen is empty. v0.2.247 adds MiniOS Streaming Player. Its window, play script, and /tmp names are its own. A word asks Radio Browser for plain MP3 or AAC stations. A pasted address plays that address. v0.2.248 also searches the Free-TV channel list. A picture channel plays the smallest size in that playlist. v0.2.249 keeps checking one more TikTok page while the chosen seconds remain, until 16 videos are ready. v0.2.250 skips a foreign title before the one-second check so the 30 second search can show more videos. The YouTube and TikTok player bars stay. The search reads lang= from the boot line. English is the start. A video whose words are in another writing system is not shown. Bing, DuckDuckGo, and Yahoo are asked in that language. More videos checks saved leftovers first. When those are gone, it reads one next page and then checks that page. Already shown clips stay off the list. The first search still stops near 8 raw links before the other sites. The playing title is drawn on the lower right of the control bar and shortened to fit. The top line stays MiniOS TikTok Player. After More videos, tt_show_stage maps the navy window again. While a video plays, that window stays behind the picture. tt_hold_list_quiet hides the stage once per list end. The first search keeps the pulled links that the clock did not check. More videos removes the ones already shown and checks the rest of that saved list before a new web search. More videos asks DuckDuckGo, Bing, and Yahoo before another Startpage page. The first search still stops at 8 raw rows before those sites. The window does not say the topic has no further videos. While the download bar is moving, the load window shows the download words. The sentence Could not load the video stays when the bar has stopped and the clip failed. Startpage gets at most one more page when fewer than 12 raw rows are in hand. DuckDuckGo, Bing, and Yahoo run only when fewer than 8 raw rows are in hand. The rest of the clock checks which files can play. While More videos is searching, status lines call tt_paint_load_line, and tt_paint_more_row does not repaint the whole window. While tt_end_hold is set, the player skips refresh_ui. Choosing a video calls `tt_hide_for_play`, which unmaps the MiniOS TikTok window and stops the 400ms wake. The player calls `tt_unmap_search` when that clip starts. Closing the player calls `tt_show_search_after_play`, which maps the same window. While More videos is on screen, `tt_end_hold` stops the player from raising its windows. `tt_paint_more_row` limits full redraws while new rows arrive. v0.2.233 added the list-end choice, and that choice stays. When the list ends, `tt_show_end_popup` offers More videos or New search. More videos writes `/tmp/minios-tt-ask` with `more` and `/tmp/minios-tt-watched.txt` for clips that started. New search writes `fresh`, closes the player, clears the search box, and focuses the MiniOS TikTok window. Loop one and loop all do not open that window. The ask file and its pid file are removed by the search window after it reads them, or by the player if the search window is not found. Pause saves the bar second and `tt_show_bar_time` puts that second back on every tick. Play counts forward from that second. A new picture clears the pause flag in `tt_begin_bar_clip`. The status line says Paused while the video is paused. The number between minus and plus is still sent once. `tt_hold_paused_bar`, `tt_apply_bar_clock`, `tt_pause_volume_and_bar`, and `tt_after_pause_toggle` remain and are not called. `tt_mark_load_start` still clears a load failure when the next download starts, and the words "Could not load the video." stay as they are. The live TikTok wrapper calls `tt_play_quiet_picture`. That uses `-demuxer lavf`, shows the saved picture at its real size, and does not draw the on-video volume or cache bar. It does not use `lowres`, `-correct-pts`, `-autosync`, framedrop, `-mc`, or `-speed`. `tt_play_sharp_picture` and `tt_play_picture_with_sound` remain and are not called. The picture reads `/tmp/minios-tt-mp.cmd`. `tt_sync_file_ok` accepts a file when the picture length matches the sound and the picture is no larger than YouTube's 426 by 240. `tt_make_sync_file` builds that file when the download is larger, and does not change the file's own timing. `tt_load_picture_libs` loads libavresample4, libavfilter4, SDL2, libv4l2, and libavdevice4 before that save, so the picture saver can start. The search window name stays MiniOS TikTok. The player bar draws the playlist title on a second line across the full width, under the CPU numbers. `minios-tt-now` is not started. Thumbnail files are named with the video id. `tt_make_clear_file` is the save that runs. It writes a 320-long-side mpeg4 picture, copies the sound, and keeps the file's own timing. `tt_make_sync_file` remains and is not called. A 720 by 1280 file does not pass, so it is not what plays. `tt_keep_open_file` moves the open file to `/tmp/minios-tt-open.mp4` before the download folder is removed, and the exit path removes that file after the picture player has been waited on. It runs `minios-tt-mplayer`. A later play takes the lock from the clip already running. Search reads a time limit from its second argument. `play_item` in `minios-tiktok.c` does not fork `minios-tiktok-play`. The player bar `start_pl_item` does. Search and a playlist row both reach that one script.

`minios-tiktok.c` (the window on the USB is the compiled `minios-tiktok`. v0.2.233 replaces this window, the TikTok player bar, and the search script. The TikTok mplayer script stays. The YouTube player bar is not replaced. `tt_take_end_ask` runs after each X event. `tt_run_more_search` sets `MINIOS_TT_WATCHED` and calls `do_search`. `tt_begin_fresh_search` clears the box. `tt_focus_query` focuses the window only after it is viewable.):

| Function | Called by | Effect |
|---|---|---|
| `main` | process start | Creates the window. `x_error` logs to `/tmp/minios-tiktok.log` and returns 0. `XSetInputFocus` runs only after `MapNotify`. Focusing before the window was viewable exited the process |
| `do_search` | Search click, when `can_search` | Forks `minios-tiktok-search`. Paints "Searching TikTok. Please wait." Lines starting `STATUS` become the status text. Cap `MAXR` is 120 |
| `thumbs_for_page` | after results, Next, Previous | `start_thumbs` forks `minios-tiktok-thumbs` for the visible page (`PAGE` is 5) |
| `play_item` | a result click | Writes `/tmp/minios-tt-playlist` and `/tmp/minios-tt-dl.progress`, starts `minios-tt-player-bar` if `pgrep_player` is false, otherwise writes `/tmp/minios-tt-mp.reload`. Does not fork `minios-tiktok-play`. `tt_hide_for_play` then unmaps the search window and clears `play_wait` |
| `release_search_ram` | `finish_app` | Kills search and thumbs and deletes search temps. Leaves a playing clip |
| `finish_app` | window close | `release_search_ram`, then forks `minios-tt-cleanup` |
| `paint`, `layout`, `draw_thumb`, `text_ellip` | expose and the idle timeout | Each row shows the title and `tiktok.com/@user/video/id`. While `play_wait` is set, reads `/tmp/minios-tt-dl.progress` and shows the label plus percent |

`minios-tiktok-search` functions: `rowcount`, `enc_form`, `extract_html`, `is_challenge`, `fetch_post`, `fetch`. Bing and Yahoo keep going until 80 rows. DuckDuckGo POST pages are `s=0,30,60,90,120,150,180` and stop at 120. Each page echoes `STATUS`. The tikwm resolve step stops after 40 new rows or 120 total. It writes `/tmp/minios-tt-rows.txt`. When `MINIOS_TT_WATCHED` names a non-empty file, `remember_watched` skips those video ids and `tt_pull_more_unseen` reads up to three more pages. A normal search does not set that variable.

`minios-tiktok-play` has `fail()`, `has_video_stream`, `pick_video_file`, and `run_ytdlp`. It refuses `/photo/`, profile, tag, music, live, explore, and login URLs. yt-dlp runs on the canonical post first. TikWM is the fallback, and its download is kept only when the HTTP status, content type, final URL, size, and video stream agree. It removes `/tmp/minios-ttplay.mp4` and runs `minios-tt-mplayer` on the file that passed `tt_sync_file_ok`. The helper path is `/opt/minios/minios-tt-mplayer`. It must not write `/tmp/minios-mp.done` or `/tmp/minios-dl.progress`.

`minios-tt-mplayer` calls `tt_play_quiet_picture`. That plays with `-demuxer lavf` and does not draw the on-video volume or cache bar. It does not use `lowres`, `-correct-pts`, `-autosync`, framedrop, `-mc`, or `-speed`. `tt_play_sharp_picture` and `tt_play_picture_with_sound` remain and are not called. The player writes `/tmp/minios-tt-mp.cmd` and the picture reads that same pipe. It writes `/tmp/minios-tt-mp.done` when the play generation still matches and the TikTok quit file is absent. It always exits 0.

`minios-tt-player-bar.cxx` is a clone of the player source. It may be compiled to `minios-tt-player-bar`. Its paths are the `tt` names: `/tmp/minios-tt-mp.cmd`, `.out`, `.pid`, `.fail`, `.geom`, `.quit`, `.done`, `.reload`, `/tmp/minios-tt-dl.progress`, `/tmp/minios-tt-playlist`, `/tmp/minios-tt-play-gen`, `/tmp/minios-tt-player-bar.pid`, `/tmp/minios-ttplay.sub`. Do not copy a TikTok edit back into `minios-player-bar.cxx`.

## 18. FLTube functions that are not the click path

On the click path: `main` → `fit_search_to_screen` → `doSearch` → `update_video_info` → `preview_video_cb` → `write_playlist` → `show_fltube_shade` → `start_minios_player`. `resize` calls `layout_fltube_chrome` → `place_result_row`.

Stock FLTube still in the binary, and not how MiniOS plays a search result:

| Function | Why it is still here |
|---|---|
| `stream`, `download_video` | Old FLTube player and saver. `ENABLE_ALTERNATIVE_STREAM_METHOD` is forced false |
| `do_youtube_search`, `search`, `retrieve_metadata`, `parse_metadata` | Search results for the four rows |
| `getNextInSearchHistory`, `getPreviousInSearchHistory`, page callbacks `getNextSearchResults_cb` and the previous/first/last twins | The search pager. Leave the four-row layout alone |
| `markLikedVideo_cb`, `add_to_watch_later`, `removeFromCache_cb`, `UserDataManager::*` | FLTube lists on disk under the home config. Not the MiniOS playlist |
| `check_fltube_update_cb`, `change_stream_resolution_cb`, `showFLTubeHelpWindow` | Upstream dialogs |
| `cache.cxx` `GeneralCache` / `PermanentDiskCache` | Thumbnail and metadata cache under `/tmp/fltube-cache` |
| `import_minios_seen`, `poll_stream_done`, `youtube_id_from_url` | MiniOS additions. `youtube_id_from_url` is what `write_playlist` uses so the index is the clicked video |

Compile notes that have already failed a bake: Tiny Core libfltk needs `-fno-rtti`. The link line needs `-lXfixes` and `-lssl -lcrypto` with an rpath-link to the unsquashed OpenSSL. `g++ -m32 -dumpmachine` can still print `x86_64-linux-gnu` while the ELF is i386. Stock `compile-fltube-i686.sh` misses those flags. Do not put the v0.2.106 FLTube binary back. The v0.2.152 layout (left margin 40, dock strip 56, thumbnail 95x80) is the one that belongs on the stick.

## 19. Other C programs

| File | Functions that matter | Who starts it |
|---|---|---|
| `minios-cad.c` | `show_notice`, `show_menu`, `do_refresh`, `run_refresh_now`, `watch`, `grab_cad_keys`, `auto_reclaim_on` | `minios-cad-init` from inittab. `--notice` is the two-line Freeing memory box. The menu runs `minios-reclaim full` |
| `minios-resmon-x11.c` | `read_stats`, `paint`, `place_bar`, `raise_no_focus`, `save_pos` | `zzzzz-minios-resmon`. Must not call `XSetInputFocus` |
| `minios-mp3.c` | `scan_all`, `start_play`, `play_next`, `send_cmd`, `reclaim_audio`, `stop_play` | `minios-mp3` / `minios-music`. Its mplayer is separate. YouTube reclaim's `pkill -x mplayer` will stop music too |
| `minios-yt-focus.c` | `find_youtube`, `click_field` | `minios-yt-focus` after YouTube opens, to put the caret in Term/URL. It does not download |
| `minios-ai-chat.c` | `paint`, `send_now`, `take_reply` | `minios-ai-chat` from the AI dock icon. Focus is taken on MapNotify. An X error does not quit |
| `minios-xvesa-wrap.c` | `main` only | Keeps Ctrl+Alt+Backspace from killing X. Installed by `minios-xvesa-install` / `minios-patch-xvesa-nozap` |
| `minios-player-keys` | key helper script | Player volume keys. Do not bind it to TikTok's fifo |

## 20. Shell functions by file

These are the functions actually defined. A script missing from this list is straight-line.

| File | Functions |
|---|---|
| `minios-run` | `auto_reclaim_on`, `wait_if_reclaiming`, `show_ram_wait`, `end_ram_wait`, `run_close_reclaim`, `stop_loading`, `alert_loading`, `fltube_up`, `app_up`, `lock_holder_alive` |
| `minios-reclaim` | `user_app_running`, `kill_by_comm`, `kill_user_apps`, `wipe_tmpfs_caches`, `is_keep_comm`, `reap_leftovers`, `drop_caches_root`, `swapoff_wait`, `reset_zram`, `reset_disk_if_high`, `kill_tiktok_only`, `kill_text_browser_only` |
| `minios-appkit` | `ram_avail_kb`, `note_ram_start`, `note_ram_stop`, `canon`, `kit_tczs`, `is_keep_tcz`, `other_kit_needs`, `load_one_tcz`, `unload_one_tcz`, `kill_kit_procs`, `wipe_kit_tmp`, `do_load`, `do_note`, `do_unload`, `do_unload_all` |
| `minios-apps` | `write_keep`, `is_keep_name`, `canon`, `do_register`, `do_unregister`, `do_status`, `kill_named`, `do_kill_all` |
| `minios-swap-wbar` | `append_icon`, `place_tiktok_beside_youtube` |
| `minios-ai-ask` | `find_py`, `fail`, `cleanup`, `run_curl` |
| `minios-sound-up` | `mount_tcz`, `mount_tree`, `unload_boot_chime`, `mod_loaded`, `load_ko` |
| `minios-create-swap` | `need_root`, `parse_size_mb`, `mem_mb`, `swap_mb`, `zram_active_mb`, `find_internal_disk`, `enlarge_zram`, `pick_swapfile`, `enable_file`, `write_conf`. Must not run from `swap-wbar.sh` and must not install onto the IDE disk (`minios-internal` is the guard) |
| `minios-wifi` | `ensure_tools`, `bring_modules`, `list_ifaces`, `scan_only`, `wired_dhcp`, `status`, `menu` |
| `minios-load-tcz` | `find_optional`, `already`, `ensure_python3_bins` |
| `minios-refresh-desktop` | `ask_mplayer_quit`, `kill_by_comm`, `clear_run_locks`, `kill_gui_apps`, `one_media_player_icon` |
| `minios-text-browser` | `find_links`, `run_text_links` |
| `minios-quick-search` | `enc`. Stays in its own aterm. Must not open Text Browser as a second window |
| `minios-tiktok-search` | `rowcount`, `enc_form`, `extract_html`, `is_challenge`, `fetch_post`, `fetch` |
| `minios-firefox` | `find_root` |
| `minios-xvesa-nozap` | `patch_one` |
| `minios-ddg` | `swap_mb`, `have_net` |
| `minios-resmon` | `read_cpu_sample`, `cpu_pct`, `ram_pct`, `zram_line`, `swap_line` |

`minios-yt-cleanup` is YouTube-only and already unmounts the Python squashfs. It must not `pkill` every `python3.9` (Nightly and the 2025 search client need it) and must not remove `/tmp/minios-tt-*`.

`minios-tt-cleanup` is TikTok-only. `finish_app` forks it, and `minios-run` also reclaims. Both may run. The cleanup must not remove `/tmp/minios-mp.*` or `/tmp/minios-yt-playlist`.

`minios-after-close` is what the player forks on Quit. It is separate from `minios-run`.

## 21. If you change this, also check that

| Edit | Also check |
|---|---|
| `write_playlist` or `/tmp/minios-yt-playlist` | Player `load_playlist`. First line is the index. A bad index plays "Search result 0" |
| `start_minios_player` | Does not write `/tmp/minios-mp.reload` on the first start. Does write it when the bar is already up. Clears `.fail`, `.quit`, and the resolve cache |
| `minios-shorts-play` | Still calls `minios-yt-dlp -g`, never `minios-yt-play`. Does not write reload. TikTok URLs still divert to `minios-tiktok-play` |
| `minios-mplayer` in `restore-106` | `LOCALYT` still skips reload. `-loop 1` stays. `-demuxer lavf` stays here only |
| `minios-tt-mplayer` | No `/tmp/minios-mp.*`. No forced lavf. Done file is `/tmp/minios-tt-mp.done` with the generation |
| `minios-player-bar.cxx` | Nothing on the laptop changes. Recompiling replaces the patched ELF |
| `swap-wbar.sh` | No `pkill -x wbar`. No swap-file creation. No empty `i/t/c` rows. One Media Player row |
| `minios-reclaim` youtube | Unmount `/tmp/minios-py311` before `rm`. Do not add TikTok process names |
| `minios-appkit` `kill_kit_procs` | TikTok branch must not gain a blanket mplayer kill while YouTube is open. YouTube branch must not start killing `minios-tt-player-bar` |
| New `sudo` command | Both lines in `minios-refresh-sudoers`, then the image file is appended, not replaced |
| `bootsync.sh` / `bootlocal.sh` | `minios-swap-early` stays before X. No `exec flwm` written to `~/.xsession`. No tce boot word |
| `wbar_setup.sh` inside `wbar.tcz` | `chown`/`chmod` of `xwbar.lst` stay silenced. The file lives on FAT |

## 22. The rest of the repository is not the Gateway USB

Changing these does not change the laptop until someone bakes them into `core.gz` or the FAT partition.

| Tree | What it is |
|---|---|
| `scripts/`, `config/`, `desktop/` | antiX 0.1 desktop (IceWM, Midori, `minios-install.sh`). `README.md` still describes that product. The Gateway does not boot it |
| `docs/` | antiX design notes plus `TINYCORE-MINIOS-DELTA.md`. Not runtime |
| `branding/` | Shared art. The USB wallpaper stamp uses `poc/tinycore` branding and `stamp-image-id.py` |
| `poc/tinycore/build-bake-ap01-vN.sh` | One bake script per image. Use the newest as the pattern. Do not re-run an old one. v149's script used a custom cpio rewriter |
| `poc/tinycore/compile-*.sh` | Host compilers. `compile-player-bar.sh` must not replace the YouTube ELF. `compile-tiktok.sh` builds `minios-tiktok`. `compile-fltube-i686.sh` needs the flags in section 18 |
| `backup/*.img` and `poc/tinycore/*.img` | Flash images. Never overwrite. Fallback is `backup/MiniOS-TC-v0.2.148-loop-off.img` |
| `versions/` | Old snapshots and notes |
| `starlight-main/`, `Starlight-MiniOS/`, `test/` | Separate LAN project (`docs/STARLIGHT-MINIOS-LAN.md`). Not in `core.gz` |
| `artifacts/`, `iso/`, `vendor/` | Unpacked trees and the antiX ISO. Not the live edit surface |
| `sessions/gateway-v02*-logs/` | Logs copied off the laptop after a test. Read them before guessing why play failed |

Host-only files inside `poc/tinycore/scripts` (never a Gateway program): `fetch-*.sh`, `prepare-b43-firmware.sh`, `make-*-icon.py`, `stamp-image-id.py`, `set-img-volname.sh`, `patch-player-loop.py`, `minios-baseline-check`, `minios-ap06-check`, `minios-check-autostart`. `LICENSE-YOUTUBE.txt` and `minios-view-image/` (a Nightly extension) ride along with Firefox settings. `minios-xsession-stock` must not be installed into `core.gz`.
