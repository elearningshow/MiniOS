# Path map for FUNCTION-MAP.md

`FUNCTION-MAP.md` was written in the development tree before this package was cut from it. Its paths differ from the paths in this package. Use this table to read it.

| Path in FUNCTION-MAP.md | Path in this package |
| --- | --- |
| `poc/tinycore/scripts/` | `source/scripts/` |
| `poc/tinycore/fltube-src/` | `source/fltube-src/` |
| `poc/tinycore/restore-106/` | `source/restore-106/` |
| `poc/tinycore/build-bake-ap01-vN.sh` | `bake/build-next-image.sh` (the v0.2.260 record is `bake/build-bake-ap01-v260.sh`) |
| The ELF player files on the image | `source/usb-binaries/` |

These places are named in `FUNCTION-MAP.md` but are not in this package:

- `sessions/CURRENT.md` and `sessions/SESSION-START.md`. Ignore the steps that refer to them. The image to flash is named in the top-level `README.md`.
- `backup/`. The next image is written to `out/`.
- The repository-root `scripts/`, `config/` and `desktop/` folders. They are the old antiX tree and were left out on purpose.

The call lists in `FUNCTION-MAP.md` were written for v0.2.155. Where it disagrees with a file in `source/scripts/`, the file wins.
