# Developing MiniOS

This page is for people who want to change MiniOS or build a new image. To just write a stick, see the [README](../README.md).

This repository is the MiniOS package. Uploading it, or forking only this folder, is enough to write the USB stick and to change the project. The rest of a development tree is not required.

The system it was built for is a Gateway MA3: one 32-bit core, about 441 MB of working memory, a 512 MB swap file, and no hardware video decoder. A change that needs a second core, 64-bit code, a hardware decoder, or a large copy in memory will not run on that machine.

The name TinyOS in older notes means Tiny Core Linux. MiniOS is not a fork of the Tiny Core GitHub group, and it is not Tianocore.

## The image in this folder

| Item | Value |
| --- | --- |
| File | `MiniOS-TC-v0.2.260-bar.img` |
| Size | 905969664 bytes (864 MiB) |
| SHA-256 | `C9237E3E63DAFBF83E152ECAE8807AD65B6075B5BB69E88B457B42F8F863875E` |
| MD5 | `FF61011583D0F0E0F6E1DC50EFCDFB0A` |
| Desktop caption | MiniOS-TC-v0.2.260 |
| Windows volume name | MINIOS-2260 |
| Base | Tiny Core Linux 16.2, 32-bit |
| Boot | Live USB only. Legacy / CSM. Not a 64-bit-only UEFI machine. |

The same hashes are in `SHA256SUMS.txt` and `VERSION.txt`.

On Windows PowerShell, from the repository folder:

```
Get-FileHash .\MiniOS-TC-v0.2.260-bar.img -Algorithm SHA256
```

The hash must match the table above. Do not format the stick after Rufus finishes, and do not delete the files Windows shows on the small visible part of the stick. The system partition starts at byte 1048576. Windows only shows the FAT boot area.

## What is in this folder

| Path | What it is |
| --- | --- |
| `MiniOS-TC-v0.2.260-bar.img` | The stick image. Flash this file. |
| `source/scripts/` | The MiniOS programs the bake copies onto the stick. Edit these. |
| `source/usb-binaries/` | The YouTube player, the YouTube mplayer wrapper, and the TikTok player bar that are on this image. Do not replace them. |
| `source/restore-106/` | Saved YouTube files from the v0.2.106 restore. The mplayer wrapper here matches the stick. The player-bar file here does not. |
| `source/fltube-src/` | Source of the YouTube search window. |
| `source/compile/` | Scripts that compile the 32-bit programs. |
| `source/tcz-office/` | AbiWord, Gnumeric, and the Tiny Core packages they need. Already inside the image. Kept so a later bake can put them back. |
| `source/icons/` | Pictures for the bottom bar, including Streaming, Document, and Spreadsheet. |
| `source/wallpaper/` | The 1792x1008 desktop picture taken from this image. |
| `source/browser-tweaks/` | Small Nightly memory notes. Not required to boot. |
| `bake/build-bake-ap01-v260.sh` | Record of the bake that made this image. Its paths point at the development machine. Do not run it from this folder. |
| `bake/build-next-image.sh` | Builds the next image from this folder. |
| `docs/WHAT-MINIOS-CHANGED.md` | What MiniOS added to Tiny Core 16.2. |
| `docs/FUNCTION-MAP.md` | How the programs call each other. Parts were written for older images, so the file in `source/scripts/` wins. |
| `docs/FILES.md` | Points to this page. Run `git ls-files` for the full list. |
| `docs/RULES.md` | Rules for a change that still runs on the Gateway. |
| `docs/KNOWN-ISSUES.md` | Problems that have been seen and are not fixed yet. |
| `out/` | Empty until a next bake. The new image is written here. |

`docs/WHAT-MINIOS-CHANGED.md` explains the Tiny Core changes. `docs/RULES.md` is the short list to read before an edit.

## Change the project and bake the next image

1. Edit the program you mean to change under `source/scripts/`. Leave the other program alone. YouTube names start with `minios-yt` or `minios-mp`. TikTok names start with `minios-tt`. Streaming names start with `minios-st` or `minios-streaming`.
2. Do not replace the three files in `source/usb-binaries/`.
3. Do not ship `source/scripts/minios-mplayer`. That wrapper is not the one on the stick. The stick wrapper is `source/usb-binaries/minios-mplayer`.
4. Do not compile `source/scripts/minios-player-bar` or `source/scripts/minios-tt-player-bar.cxx` and put the result on the stick.
5. On a machine with WSL, from the repository folder, run:

```
wsl -u root bash bake/build-next-image.sh
```

The script must be run as root. It reads `MiniOS-TC-v0.2.260-bar.img`, copies the programs in `source/scripts/` into a new image, and writes:

- `out/MiniOS-TC-v0.2.261-edit.img`
- `out/MiniOS-TC-v0.2.261-edit.img.copy`

The `.copy` file is the check that the write matched. After the script prints `BAKE_NEXT_OK` and `COPIES_MATCH`, the `.copy` file can be deleted. The script refuses to overwrite `MiniOS-TC-v0.2.260-bar.img`.

The next image caption is MiniOS-TC-v0.2.261. The volume name is MINIOS-2261. To use another number, edit the `VER`, `IMG_NAME`, and `CAPTION` lines at the top of `bake/build-next-image.sh`, the kernel line that changes `0.2.260` into the new number, and the volume text `MINIOS-2261`. The volume name is `MINIOS-` plus the minor number and the patch number, and it must be 11 characters or fewer. `0.2.261` becomes `MINIOS-2261`.

Several player files are locked to a checksum inside the bake script. If you change one of those files, the bake stops until the matching checksum in `bake/build-next-image.sh` is updated in the same edit. The locks exist so a dock change cannot quietly replace the YouTube or TikTok player. The dock script, the bar starter, the boot scripts, and the Streaming programs are the files a menu or Streaming change normally edits.

MiniOS-TC-v0.2.260 has been booted and tested on the Gateway MA3. The desktop and most programs work, but the Spreadsheet button can fail to open. The same problem is on the v0.2.261-edit test build. It is a known issue and has not been fixed. See `docs/KNOWN-ISSUES.md`.

## How the stick is built

Tiny Core's own files and the MiniOS programs live in one image.

- Byte 1048576 is the start of the FAT partition.
- The FAT area holds the boot files and `cde/optional/*.tcz`. Those are Tiny Core extensions. AbiWord and Gnumeric are in that folder and are not loaded at boot.
- `boot/core.gz` is the root filesystem. MiniOS programs are installed under `/opt/minios/` inside it.
- The bottom bar program is Tiny Core `wbar`. The script that starts it on this image is `source/scripts/wbar-wait.sh`, installed as `/usr/local/bin/wbar.sh` inside `wbar.tcz`.
- The desktop file `/etc/skel/.xsession` comes from Tiny Core Xlibs. It starts `wbar.sh`, then runs each file in `/usr/local/etc/X.d`. One of those files runs `/opt/minios/swap-wbar.sh`, which is `source/scripts/minios-swap-wbar`.
- On this image `wbar.sh` builds the icon list and then draws the bar once. `swap-wbar.sh` writes the icons, including Streaming, and does not start the bar a second time.

A new image is made by copying this image and replacing files inside `core.gz` and `wbar.tcz`. It is not made by recompiling Tiny Core from the upstream ISO. The upstream 16.2 files are at http://tinycorelinux.net/16.x/x86/ . The kernel sources are under `release/src/` on that site. Tiny Core's GitHub group, https://github.com/tinycorelinux , is a set of small repositories. It is not this system, and this project does not fork it.

## Git LFS

The image is stored with Git LFS (`*.img` is listed in `.gitattributes`). Install Git LFS before cloning or pushing, or the image downloads as a small pointer file. The other files are small enough for a normal commit. `source/tcz-office/` is about 55 MB spread across many files. To only write a stick, download the image from the release page instead of cloning, because Git LFS downloads count against a monthly bandwidth quota on GitHub.

## Left out on purpose

- Older stick images. They are not needed to flash this one or to bake the next one.
- The experimental local language-model folder (`llamafile` and `model.gguf`). That test is not part of the image. It is too heavy for 441 MB.
- Session transcripts and the long history of earlier bakes.
- The old antiX tree that used to live at the repository root. It is not on the USB.
- The Tiny Core 16.2 ISO. This image already contains that system plus the MiniOS files.
