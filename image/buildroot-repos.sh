#!/bin/sh
# Runs INSIDE sdk/target; the build copy of the 5.2.0.22 rootfs is at /build. Plain signed Jolla repos instead of plugin:ssu.
set -e
R=/build; V=5.2.0.22
Z="zypper --non-interactive --gpg-auto-import-keys --root $R"
$Z rr jolla hotfixes adaptation-common apps native-common 2>/dev/null || true
$Z ar -f https://releases.jolla.com/releases/$V/jolla/aarch64/ jolla
$Z ar -f https://releases.jolla.com/releases/$V/hotfixes/aarch64/ hotfixes
$Z ar -f https://releases.jolla.com/releases/$V/jolla-hw/adaptation-common/aarch64/ adaptation-common
$Z ar -f https://releases.jolla.com/jolla-apps/$V/aarch64/ apps
$Z ar -f -G https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/ native-common
$Z ref
$Z in --no-recommends gcc gcc-c++ make meson ninja pkgconfig autoconf automake libtool \
  glib2-devel libqmi-devel protobuf-devel protobuf-compiler \
  qt5-qmake qt5-qtcore-devel qt5-qtdbus-devel qt5-qtnetwork-devel sensorfw-qt5-devel rpm-build git
echo BUILDROOT-READY
