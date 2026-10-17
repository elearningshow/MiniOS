# Known issues

## Spreadsheet (Gnumeric) closes right after it opens

Status: accepted, not fixed.

On v0.2.260 (the packaged image) and on the v0.2.261-edit build, the Spreadsheet window opens and closes right away. Document and the other buttons work. Replacing Gnumeric and the packages it needs with fresh copies from the Tiny Core 16.x repository and baking a new image did not change it. The cause is not known. Use Document for text work.

To investigate later: click Spreadsheet once, let it close, then open a terminal and run:

```
cat /tmp/minios-load-tcz.log
dmesg | tail -30
gnumeric 2>&1 | head -30
```

The log shows whether the package loaded. Running `gnumeric` by hand shows its own error message.

## First start can sit on a blue screen

A new stick can sit on a blue screen after “Setting hostname to box.” If it is still there after a few minutes, power off and boot the same stick again.
