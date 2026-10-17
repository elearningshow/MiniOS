# MiniOS

![MiniOS desktop](source/wallpaper/mini-os-bg.jpg)

MiniOS Xperience is a live USB system for an old 32-bit PC, built on Tiny Core Linux 16.2. It starts from the stick and does not install onto the internal hard drive. It was built for a Gateway MA3: one 32-bit core, about 441 MB of working memory, and no hardware video decoder.

## Download

Get the image from the [v0.2.260 release page](https://github.com/elearningshow/MiniOS/releases/tag/v0.2.260). Download `MiniOS-TC-v0.2.260-bar.img` and `SHA256SUMS.txt`.

SHA-256 of the image:

```
C9237E3E63DAFBF83E152ECAE8807AD65B6075B5BB69E88B457B42F8F863875E
```

To check your download on Windows, run `certutil -hashfile MiniOS-TC-v0.2.260-bar.img SHA256` and compare the result.

## Write the stick

Use a USB stick of at least 1 GB. Do not select the internal disk.

1. Download Rufus from https://rufus.ie and start it as administrator.
2. Insert the USB stick and select it.
3. Click SELECT and choose `MiniOS-TC-v0.2.260-bar.img`.
4. When Rufus asks how to write the image, choose **DD Image**. Do not choose ISO mode.
5. Click START and wait until Rufus says READY.
6. If Windows asks to format the stick, click Cancel.

On Linux, find the stick with `lsblk` and use the disk name, not a partition:

```
sudo dd if=MiniOS-TC-v0.2.260-bar.img of=/dev/sdX bs=4M status=progress conv=fsync
```

## Boot

1. Leave the stick inserted and turn the PC on.
2. Open the boot menu (often F10, F12, or Esc) and choose the USB stick. Use Legacy / CSM mode, not UEFI only.
3. **Start MiniOS** is already selected and starts after a few seconds.
4. **MiniOS command line only** is for a desktop that does not start. The user name is `tc`. Press Enter at the password prompt.
5. The corner of the desktop reads **MiniOS-TC-v0.2.260**.

## The desktop

A row of buttons runs along the bottom.

| Button | What it does |
| --- | --- |
| YouTube | Search YouTube and play a result. |
| TikTok | Search TikTok and play a result. |
| Streaming | Search free radio and TV stations. |
| Nightly | Web browser. JavaScript starts off. |
| Text Browser | A text-only browser. |
| Media Player | Play a local video or music file. |
| Games | GTK Tetris and LBreakout2. The first use downloads about 2.6 MB. |
| AI | A chat window. It needs an Internet connection. |
| Document | AbiWord word processor. |
| Spreadsheet | Gnumeric. Known issue, see below. |
| Wifi | Connect to a wireless network (Broadcom 4318). |
| Refresh RAM | Free memory after a heavy program. |
| Exit | Leave MiniOS. |

Open one program at a time on the Gateway.

## Known issues

- The Spreadsheet button opens and closes right away. Document and the other buttons work.
- The first start of a new stick can sit on a blue screen. Power off and boot the same stick again.

Details are in [docs/KNOWN-ISSUES.md](docs/KNOWN-ISSUES.md).

## Change or rebuild MiniOS

See [docs/DEVELOPING.md](docs/DEVELOPING.md) for the folder layout, how to bake a new image, and how the stick is built.

## License

The MiniOS programs and documents are under the MIT License. See [LICENSE](LICENSE). Tiny Core Linux and the packages in the image keep their own licenses.
