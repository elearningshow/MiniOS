# Known issues

## Spreadsheet (Gnumeric) may not open

On v0.2.260 (the packaged image) and on the v0.2.261-edit build, the Spreadsheet window can close right after it opens. Document and the other buttons work. Because both images show it, the bake script is not the cause.

**Recommended fix: reinstall Gnumeric.** This needs the network and has not been confirmed to work on every stick.

1. Connect Wifi with the Wifi button, then open a terminal as the `tc` user.
2. Remove Gnumeric:

```
T=$(readlink -f /etc/sysconfig/tcedir)
sudo umount /tmp/tcloop/gnumeric 2>/dev/null
rm -f $T/optional/gnumeric.tcz*
```

3. Install it again and start it:

```
tce-load -wi gnumeric.tcz
df -h $T
gnumeric
```

Check `df -h` first: the stick's boot partition is nearly full.

## First start can sit on a blue screen

A new stick can sit on a blue screen after “Setting hostname to box.” If it is still there after a few minutes, power off and boot the same stick again.
