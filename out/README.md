# Output

This folder is empty until `bake/build-next-image.sh` runs.

The script writes `MiniOS-TC-v0.2.261-edit.img` and `MiniOS-TC-v0.2.261-edit.img.copy` here. The `.copy` file is the check that both writes match. After `COPIES_MATCH`, the `.copy` file can be deleted.

Do not put `MiniOS-TC-v0.2.260-bar.img` in this folder. That file stays at the top of the package.
