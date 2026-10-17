# Files in this package

The table "What is in this folder" in the top-level `README.md` is the guide to this package. It names each folder and says what it is for.

For an exact, current list of every file, run this from the repository folder:

```
git ls-files
```

Top-level folders:

- `source/` holds the MiniOS programs (`scripts/`), the saved player files that are on the image (`usb-binaries/`), the YouTube search window source (`fltube-src/`), compile scripts, icons, the wallpaper, and the office packages.
- `bake/` holds the script that builds the next image and the record of the bake that made this one.
- `docs/` holds these notes.
- `out/` is where a new image is written. It is empty until a bake.

This page used to list every file with its size. That list went out of date after each commit, so it was removed.
