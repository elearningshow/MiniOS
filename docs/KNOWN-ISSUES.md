# Known issues

Problems that have been seen and are not fixed yet.

## Spreadsheet (Gnumeric) closes right after it opens on v0.2.261-edit

Status: open. Seen on 2026-10-17 on `out/MiniOS-TC-v0.2.261-edit.img`, which `bake/build-next-image.sh` built from the packaged v0.2.260 image. Tested on the Gateway MA3.

What happens: click Spreadsheet. The window tries to open and then closes. Document (AbiWord) and the other buttons work.

Messages seen on the screen or in the log:

- `xvid-dev.tcz not found`
- `alsa.tcz load failed`
- a Gnumeric `input/output error`
- a `loop-mount` line near the bottom of the output

What the messages mean, from `source/scripts/minios-load-tcz`: the script first runs Tiny Core `tce-load` on `gnumeric.tcz`, which also loads every package it depends on. When that fails, the script logs `tce-load failed ... - loop-mount` and mounts `gnumeric.tcz` on its own, without `goffice`, `gtk3`, `libgsf` and the other packages it needs. Gnumeric then starts without its libraries and exits.

Not known yet:

- why `tce-load` fails. `xvid-dev.tcz` and `alsa.tcz` are not named in the files that were read, and Spreadsheet has no entry in `source/scripts/minios-appkit`.
- which `.dep` file names `xvid-dev.tcz`.
- whether the packaged v0.2.260 image shows the same failure.

Checks to run on a PC with WSL (as root), with the new image mounted:

```
mkdir -p /mnt/img
mount -o loop,ro,offset=1048576 out/MiniOS-TC-v0.2.261-edit.img /mnt/img
cd /mnt/img/cde/optional
for f in *.md5.txt; do md5sum -c "$f" 2>&1 | grep -v ': OK'; done
grep -l -e xvid -e alsa *.dep
cd /
umount /mnt/img
```

Repeat with `MiniOS-TC-v0.2.260-bar.img` to see whether the packaged image differs. On the stick, read `/tmp/minios-load-tcz.log` and `/tmp/minios-appkit.log` after a failed click.

Workaround: none known. Use Document for text work.

## First start can sit on a blue screen

A new stick can sit on a blue screen after “Setting hostname to box.” If it is still there after a few minutes, power off and boot the same stick again. This is a known first-boot behavior and has not been fixed.
