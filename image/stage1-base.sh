#!/bin/sh
# Runs INSIDE the SDK target chroot (sdk/target). Target rootfs is bind-mounted at /sfos.
set -e
R=/sfos
V=${V:-5.2.0.22}
mkdir -p $R/usr/bin $R/etc
cp /usr/bin/qemu-aarch64-static $R/usr/bin/
Z="zypper --non-interactive --gpg-auto-import-keys --root $R"
$Z rr jolla hotfixes adaptation-common apps native-common 2>/dev/null || true
$Z ar -f https://releases.jolla.com/releases/$V/jolla/aarch64/ jolla
$Z ar -f https://releases.jolla.com/releases/$V/hotfixes/aarch64/ hotfixes
$Z ar -f https://releases.jolla.com/releases/$V/jolla-hw/adaptation-common/aarch64/ adaptation-common
$Z ar -f https://releases.jolla.com/jolla-apps/$V/aarch64/ apps
$Z ar -f -G https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/ native-common
$Z ref
$Z in --no-recommends \
  patterns-sailfish-core-device patterns-sailfish-mw patterns-sailfish-ui \
  patterns-sailfish-device-configuration-common patterns-sailfish-applications \
  patterns-sailfish-cellular-apps patterns-sailfish-consumer-generic \
  jolla-settings-networking-multisim jolla-settings-system-orientation \
  mesa-dri-drivers mesa-libEGL mesa-libGLESv2 mesa-libgbm wayland-egl \
  qt5-plugin-platform-eglfs qt5-qtwayland-wayland_egl qtscenegraph-adaptation \
  usb-moded alsa-ucm-conf openssh-server jolla-developer-mode kmod
