# Baking the next image

`build-bake-ap01-v260.sh` is the record of how `MiniOS-TC-v0.2.260-bar.img` was produced from the previous image on the development machine. The paths inside it point at that machine. Do not run it from this folder.

`build-next-image.sh` is the script to run here.

```
wsl -u root bash bake/build-next-image.sh
```

It has to run as root because it mounts the image. It reads `MiniOS-TC-v0.2.260-bar.img` and writes `out/MiniOS-TC-v0.2.261-edit.img` plus a `.copy` used to check the write. It does not overwrite the packaged image.

Edit `source/scripts/` before you run it. The script stops if a locked player file no longer matches its checksum. That stop is intentional. Update the checksum in this script in the same edit, or put the player file back.

The base caption check looks for `MiniOS-TC-v0.2.260` inside the packaged image. The kernel line changes `0.2.260` to `0.2.261`. If you pick a different version, change those strings together with `VER`, `IMG_NAME`, `CAPTION`, and the `MINIOS-2261` volume text. The volume comes from the caption: `0.2.261` becomes `MINIOS-2261`.

A successful run prints `BAKE_NEXT_OK` and `COPIES_MATCH`. Judge the bake by those lines and a root exit of 0. A later command that only appends text can hide a failed mount.
