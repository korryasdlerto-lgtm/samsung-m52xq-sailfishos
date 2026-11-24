# Build

All programs are built in a Sailfish OS 5.2 aarch64 build root (zypper `--root` from the Jolla repositories, see
`../image/buildroot-repos.sh`), with qemu-user on the host. The scripts run **inside** the build root and expect the sources
under `/root/src` and the output under `/root/out`.

- `ofono/`: `build-ofono.sh` builds ell 0.70 (kernel.org) and ofono 2.12 (sailfishos-native/ofono 4c79652, upstream 2.12) with the
  patches in `series`: the Sailfish rpm patches 0001–0017, the moWerk pro1x QRTR patches 0019–0022 and ours 0023–0029
  (second modem, LTE attach, IPA MAPv5, data on SIM 2). `build-ofono-incremental.sh` applies the last patch on a patched tree.
- `sensors/`: `build-sensors-1.sh` (protobuf-c, libssc 0.4.4 + the stk3a0x patches), `build-sensors-2.sh` (sensorfw SSC plugins).
- `gps/`: `build-gps.sh` (geoclue-provider-hybris with the QMI LOC backend; XTRA servers in `/etc/gps_xtra.ini`).
- `camera/libcamera/`: `build-libcamera.sh` applies the numbered patches to libcamera 0.8.0 and builds the simple and uvcvideo
  pipelines with the software ISP. The IPA module is signed with a key of the build: install `libcamera.so` and
  `ipa_softisp.so` from the same build. `build-libcamera-agc.sh` is the incremental build of the exposure patches.
- `camera/qtmultimedia/`: `qtmultimedia-v4l-m52xq.patch` is applied to [mlehtima/qtmultimedia](https://github.com/mlehtima/qtmultimedia)
  v4l c98eb6d; it contains the 15 camera patches of moWerk's sailfishos-pro1x-mainline and ours. `build-qtmultimedia.sh`
  builds everything, `build-qgsttools.sh` and `build-camerabin.sh` only those parts.
- `camera/rawphoto/`: `m52xq-rawphoto` (libcamera, GLES); `camera/jolla-camera/`: patches of the Jolla camera QML.
- `browser/`: `sailjail-browser.sh` adds the browser settings to the sandbox profile (`vblank_mode=0`, shader cache on disk).
- `tools/`: small C programs (GLib): `gcc -O2 -o m52xq-flashlight m52xq-flashlight.c $(pkg-config --cflags --libs gio-2.0)`;
  `m52xq-bt-awake` needs `gio-2.0 gio-unix-2.0`; `m52xq-gps-sv` has no dependencies.
