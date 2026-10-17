# Known issues

Problems that have been seen and are not fixed yet.

## Spreadsheet (Gnumeric) closes right after it opens on v0.2.261-edit

Status: open. Seen on 2026-10-17 on `out/MiniOS-TC-v0.2.261-edit.img`, which `bake/build-next-image.sh` built from the packaged v0.2.260 image. Tested on the Gateway MA3.

What happens: click Spreadsheet. The window tries to open and then closes. Document (AbiWord) and the other buttons work. Running `gnumeric` alone from a terminal gives an `input/output error`. Running `tce-load` on it says the package is already installed.

Messages seen on the screen or in the log:

- `xvid-dev.tcz not found`
- `alsa.tcz load failed`
- a Gnumeric `input/output error`
- a `loop-mount` line near the bottom of the output

What the messages mean, from `source/scripts/minios-load-tcz`: the script first runs Tiny Core `tce-load` on `gnumeric.tcz`, which also loads every package it depends on. When that fails, the script logs `tce-load failed ... - loop-mount` and mounts `gnumeric.tcz` on its own, without `goffice`, `gtk3`, `libgsf` and the other packages it needs. That leftover mount is probably why `tce-load` later says "already installed". It does not mean Gnumeric works.

Not known yet:

- why `tce-load` fails. `xvid-dev.tcz` and `alsa.tcz` are not named in the files that were read, and Spreadsheet has no entry in `source/scripts/minios-appkit`.
- which `.dep` file names `xvid-dev.tcz`.
- whether `gnumeric.tcz` on the stick is damaged (an input/output error on the program itself points that way), or whether the stick is failing, or whether the 441 MB of RAM is the limit.
- whether the packaged v0.2.260 image shows the same failure.

Possible workaround: see the reinstall steps below. They have not been tested and are not confirmed to work.

### Find out what is wrong first

On the stick, after a fresh reboot, open a terminal and run:

```
free -m
dmesg | tail -30
T=$(readlink -f /etc/sysconfig/tcedir)
cd $T/optional && md5sum -c gnumeric.tcz.md5.txt
ls -l gnumeric.tcz
```

- `dmesg` lines with `squashfs`, `loop`, or `I/O error` point to a damaged package or a failing stick.
- A checksum line that does not say `OK` means `gnumeric.tcz` is damaged.
- A smaller `gnumeric.tcz` than the copy in `source/tcz-office/` means it was cut short.
- `Out of memory` in `dmesg` means the RAM is the limit.

### Uninstall and reinstall Gnumeric (untested)

Tiny Core's `tce-load` has no force switch. To force a fresh install, unload the package, delete it, and download it again. This needs the network, so connect Wifi first. Run these as the `tc` user in a terminal.

Uninstall:

```
T=$(readlink -f /etc/sysconfig/tcedir)
sudo umount /tmp/tcloop/gnumeric 2>/dev/null
rm -f $T/optional/gnumeric.tcz*
```

Reinstall and start it:

```
tce-load -wi gnumeric.tcz
df -h $T
gnumeric
```

To also replace every package Gnumeric depends on, run this after the uninstall step and before the reinstall step:

```
for p in $(cat $T/optional/gnumeric.tcz.dep); do sudo umount /tmp/tcloop/${p%.tcz} 2>/dev/null; rm -f $T/optional/$p*; done
```

Run the dependency step before deleting the `.dep` file, or read the list into a variable first. Because the uninstall step above removes `gnumeric.tcz.dep`, do the dependency loop first if you want both.

Cautions:

- Do this on a spare copy of the stick. Keep a v0.2.260 stick untouched as the fallback.
- The FAT partition was already close to its 85% limit. Check `df -h` before and after. Downloaded `-dev` packages can fill it.
- Whether downloaded files stay on the stick after a reboot has not been confirmed. Check with `ls $T/optional/gnumeric.tcz` after rebooting.
- If the same input/output error comes back with a freshly downloaded file, the cause is more likely the stick or the mount than the package. Try another stick.

A lasting repair belongs in the repo: put the good `gnumeric.tcz` and any missing dependencies into `source/tcz-office/`, add them to the office list in the bake script, bake again, and check `md5sum` on the result.

## First start can sit on a blue screen

A new stick can sit on a blue screen after “Setting hostname to box.” If it is still there after a few minutes, power off and boot the same stick again. This is a known first-boot behavior and has not been fixed.
