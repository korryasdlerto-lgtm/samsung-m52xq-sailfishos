# samsung-m52xq-sailfishos

**Sailfish OS 5.2** (native, **no libhybris**, no Android container) on a **mainline Linux kernel** for the
**Samsung Galaxy M52 5G** (codename `m52xq`, SM-M526B / SM-M526BR, Qualcomm Snapdragon 778G / SM7325, platform "yupik").

It is not Halium, not libhybris and not the Android kernel. The phone runs a mainline Linux 7.1.13 kernel with device drivers
and fixes. Graphics go through Mesa freedreno, the modem through ofono's QRTR/QMI drivers, the camera through libcamera. The
root is Sailfish OS 5.2.0.22, installed with zypper from the Jolla repositories. The hardware layer (kernel, `m52xq-*`
services) comes from the postmarketOS port [samsung-m52xq-mainline](https://github.com/korryasdlerto-lgtm/samsung-m52xq-mainline)
and the other mainline ports of this phone, adapted to Sailfish (ofono, connman, MCE, PulseAudio + ohm, sensorfw, geoclue).
Personal, unofficial project (not a Jolla release), tested on one SM-M526BR.

**Status (release "v1", 2026-11-24, see [changes](docs/CHANGES-v1.md) / [на русском](docs/CHANGES-v1-RU.md)):** calls, SMS and
mobile data on two SIM cards, Wi-Fi, Bluetooth, the camera with 64 MP photos and flash, sleep, sound, sensors and a
GPU-accelerated Lipstick work. GPS does not find a position at the moment (see the known issues).

## Repositories

| | |
|---|---|
| This repository | installer, rootfs overlay (scripts, services, QML, configs), patches and build scripts of the rebuilt programs, image scripts, kernel config, documentation |
| Kernel source | [sc7280-mainline-linux-m52xq](https://github.com/korryasdlerto-lgtm/sc7280-mainline-linux-m52xq) (7.1.13), see [`kernel/`](kernel) |
| Kernel binaries | in the flashable zip (`firmware/`): boot without the IPA firmware, vendor_boot, dtbo |

## What works

| Feature | Status | Notes |
|---|---|---|
| Boot | works | pmOS initramfs, GPT image on `system` (p1 pmOS_boot, p2 pmOS_root); `/home` on the userdata partition |
| Display, GPU | works | Lipstick on eglfs_kms (`EGL_PLATFORM=drm`), Mesa freedreno (Adreno 642L), scale 1.75, auto-rotation, auto-brightness |
| Touchscreen, buttons | works | Synaptics; volume keys, volume bar, screenshot with both volume keys (also in the Camera) |
| Wi-Fi | works | WCN6750 / ath11k; MAC from the phone's EFS; stays connected in the sleep and wakes the phone |
| Bluetooth | works | address from EFS; calls and music; a connected headset keeps the phone awake; off while the screen is off and nothing is connected |
| Calls, SMS | works | **dual SIM** (both active): ofono 2.12 with QRTR; ringtone, earpiece, loudspeaker, call volume keys |
| Mobile data | works | on both SIM cards; the data SIM is chosen in the SIM manager and in the Sailfish settings |
| SIM manager | works | own Silica app: both SIMs / only SIM 1 / only SIM 2, data SIM, SIM cards off/on |
| Audio | works | UCM for the WCD938x codec, TFA9878 loudspeaker, PulseAudio + ohm policy, shutter sound |
| Proximity in calls | works | screen black and touch locked at the ear (`m52xq-incall-proximity`) |
| Flashlight | works | toggle in the top menu (`m52xq-flashlight`) |
| Sensors | works | accelerometer, light, proximity (libssc + sensorfw SSC plugins) |
| Camera | works | four cameras with a switch (0.5 ultra wide, 1.0 main, M macro, front); photos of 64 MP (main) and 32 MP (front), flash, autofocus, zoom up to 8x |
| Sleep | works | s2idle with Wi-Fi wake-up; in two tests of 7–10 minutes the phone slept 90–92 % of the time; the night has not been measured yet |
| Notifications | works | messages do not switch the display on |
| Battery, charging | works | SM5714 fuel gauge and charger |
| GPS | **no position** | no position at the moment; it worked with an earlier build of this port. The cause is not known; see the known issues |
| Video codecs (Venus) | partial | modules load; hardware decoding and recording in the Camera are not tested |
| VoLTE, fingerprint | no | calls use 2G/3G |

### Known issues

- **GPS:** the receiver starts and accepts the assistance data, but no position was found in the last tests (two kernels, indoors at a window). Local interference cannot be excluded. Online assistance (Settings → Location) is needed for a quick start.
- **Photos:** blown highlights are tinted magenta; a face against a bright window is dark; the software ISP has no noise
  reduction, so photos are less sharp than the stock camera. The main camera makes 4:3 photos while the viewfinder shows 16:9.
- **Sleep:** something of Sailfish (dsme) wakes the phone about once a minute; it goes back to sleep at once.
- **Mobile data:** the data contexts come up on both SIM cards; the tests with the operators at hand could only reach their
  portal pages (no balance), so a normal internet connection over mobile data has not been confirmed.

## Install

Requirements: unlocked bootloader, TWRP for m52xq, a microSD card with the zip, backups of EFS, persist, modem, boot,
vendor_boot and vbmeta. If you are not comfortable restoring a Samsung phone with Odin, do not install this.

- Flashable zip: `sailfishos-5.2-m52xq-mainline-v1.zip` (620 MB), md5 `af0e8264a4ae7f3d0f6ad627e7004658` (not published yet).

TWRP steps:

```
Wipe - Format Data - yes - enter - Back - Back - Advanced Wipe - Data - Repair or Change File System - Change File System - EXT4 - Swipe to Change - Back - Back - Back - Back - Back - Install - Select Storage - Micro SD card - sailfishos-5.2-m52xq-mainline-v1.zip - Swipe to confirm Flash - Reboot System
```

The installer ([`installer/`](installer)) writes boot, vendor_boot and dtbo, grows `system` with lptools and writes the root
image to it. The Samsung/Qualcomm device firmware is **not** in the zip: the installer copies it, read-only, from the phone's
own partitions. ADSP, GPU zap shader, IPA (mobile data) and Venus come from `apnhlos`, the modem from `modem`, the Bluetooth
firmware and sensor configs from `vendor`, the sensor registry from `persist`. The IPA firmware is added to the boot ramdisk.
Bluetooth and Wi-Fi addresses are read from EFS at every boot. The machine ID is created by the installer, the SSH host keys
and the user (`defaultuser`) at the first boot; then the Sailfish start wizard runs. `userdata` must be ext4: it becomes `/home`.

If `/data/rootfs.img` exists, the initramfs boots that file instead of `system` (the installer warns about it).

- SSH over USB (`172.16.42.1`) and Wi-Fi: user `root`, password `m52xq`. Change it after the first boot.

Back to Android: flash LineageOS or UN1CA in TWRP, or the stock firmware with Odin (BL, AP, CP, CSC).

## Repository layout

| Path | Content |
|---|---|
| `installer/` | `META-INF/.../update-binary` of the flashable zip (TWRP shell installer) |
| `rootfs-overlay/` | device layer over the Sailfish root: `m52xq-*` scripts and services, compositor environment, camera tuning, UCM, ohm/PulseAudio configs, QML apps. Binaries, kernel modules, firmware and two proprietary Jolla files are not here: see [`EXCLUDED-FILES.txt`](EXCLUDED-FILES.txt) |
| `build/` | patches and build scripts of every rebuilt program (table below) |
| `image/` | scripts that build the Sailfish root from the Jolla repositories; `image/release/`: device layer, snapshot and cleanup |
| `kernel/` | kernel notes, config, list of the modules in the image |
| `docs/` | manuals (EN/RU), changes of v1, step-by-step build commands and plans (Russian) |

### Rebuilt programs (`build/`)

| Folder | What and why |
|---|---|
| `ofono` | ofono 2.12 (sailfishos-native) with the Sailfish patches, the QRTR patches of the pro1x port (0019–0022) and our 0023–0029: second SIM, LTE data already attached, IPA MAPv5 data path, data on SIM 2 with its own mux links |
| `sensors` | libssc patches (Samsung stk3a0x proximity), sensorfw SSC plugins |
| `gps` | geoclue-provider-hybris with the QMI LOC backend |
| `camera/libcamera` | libcamera 0.8.0: software ISP patches of the other M52 ports (output size limits, quad Bayer, NV12, sensor info, link handling), the exposure loop rewritten so that it does not oscillate, no default exposure at the start of the stream |
| `camera/qtmultimedia` | QtMultimedia of mlehtima (v4l branch) with the patches of the pro1x port (moWerk) and ours as one patch: own autofocus and zoom for libcamerasrc, 64 MP photos through a helper, front/back detection of libcamera 0.8, flash |
| `camera/rawphoto` | `m52xq-rawphoto`: full-resolution raw capture and development on the GPU |
| `camera/jolla-camera` | small patches of the Jolla camera QML (camera list and order, volume keys, sandbox permission for the flash) |
| `tools/m52xq-flashlight`, `tools/m52xq-bt-awake` | flashlight/flash service (D-Bus), Bluetooth in the sleep |
| `tools/m52xq-gps-sv` | reads the number and strength of the visible satellites from the modem (no position), for debugging |

## Building

- Root: `image/` (stage scripts, run in a Sailfish SDK target chroot), then `image/release/assemble.sh` applies the device layer.
- Programs: in a Sailfish 5.2 aarch64 build root, see [`build/README.md`](build/README.md).
- Kernel: `make ARCH=arm64 LLVM=1 Image` with clang/LLD 22 and `kernel/config-7.1.13`; the sleep of Sailfish needs
  `CONFIG_PM_AUTOSLEEP` and `CONFIG_PM_WAKELOCKS`.
- Step-by-step commands (Russian): [docs/ИНСТРУКЦИЯ.md](docs/ИНСТРУКЦИЯ.md).

## Manuals

| Language | Markdown |
|---|---|
| English | [docs/MANUAL-EN.md](docs/MANUAL-EN.md) |
| Русский | [docs/MANUAL-RU.md](docs/MANUAL-RU.md) |

## Bug reports

Please write your model (SM-M526B / SM-M526BR), what you did and what happened. Logs help a lot: `journalctl -b` and `dmesg`
over SSH. Remove phone numbers and addresses from the logs before you send them.

## Credits

Jolla and the Sailfish OS community; moWerk ([sailfishos-pro1x-mainline](https://github.com/moWerk/sailfishos-pro1x-mainline)) for
the approach, the QRTR ofono patches and the QtMultimedia camera patches; mlehtima for the v4l QtMultimedia branch; the
libcamera, postmarketOS and Linaro/Qualcomm upstream developers.

Author: Asdlerto Korry
