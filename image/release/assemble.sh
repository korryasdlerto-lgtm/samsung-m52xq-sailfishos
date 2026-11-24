#!/bin/sh
# Device layer of the release on top of a clean Sailfish root: files, ofono, camera, rpms. Run as root. Usage: assemble.sh <mounted p2>
set -e
# Inputs (next to this script): rootfs-files/ (device layer), hw-ofono/ofonod, hw-camera/, hw-venus/, hw-sound/, rpms-camera/,
# and the scripts of build/camera/jolla-camera and messages-no-display-on.sh
B=$(cd "$(dirname "$0")" && pwd); R="$1"; cd "$B"
[ -f "$R/etc/passwd" ] && [ -d "$R/usr/lib64/qt5" ]
echo "== rootfs-files (one by one, root:root)"
cd "$B/rootfs-files"
find . -type d | while read -r d; do [ -d "$R/$d" ] || install -d -o root -g root -m 755 "$R/$d"; done
find . -type f | while read -r f; do m=$(stat -c %a "$f"); install -o root -g root -m "$m" "$f" "$R/$f"; done
find . -type l | while read -r l; do t=$(readlink "$l"); rm -f "$R/$l"; ln -s "$t" "$R/$l"; chown -h root:root "$R/$l"; done
mkdir -p "$R/mnt/userdata"
echo "== hw-ofono"
install -o root -g root -m 755 "$B/hw-ofono/ofonod" "$R/usr/sbin/ofonod"
echo "== hw-camera"
cd "$B/hw-camera"
find . -type d | while read -r d; do [ -d "$R/$d" ] || install -d -o root -g root -m 755 "$R/$d"; done
find . -type f | while read -r f; do m=$(stat -c %a "$f"); install -o root -g root -m "$m" "$f" "$R/$f"; done
find . -type l | while read -r l; do t=$(readlink "$l"); rm -f "$R/$l"; ln -s "$t" "$R/$l"; chown -h root:root "$R/$l"; done
echo "== hw-venus, hw-sound"
for d in hw-venus hw-sound; do cd "$B/$d"; find . -type f | while read -r f; do install -d -o root -g root -m 755 "$R/$(dirname "$f")"; install -o root -g root -m 644 "$f" "$R/$f"; done; done
echo "== CaptureView.qml, camera sandbox, messages"
Q="$R/usr/lib64/qt5/qml/com/jolla/camera/capture/CaptureView.qml"
for p in captureview-volume-keys captureview-cameras captureview-camera-order; do python3 "$B/jolla-camera/$p.py" "$Q"; done
md5sum "$Q"
sh "$B/jolla-camera/sailjail-camera-flash.sh" "$R"
sh "$B/messages-no-display-on.sh" "$R"
echo "== rpms-camera"
mkdir -p "$R/tmp/rpms"; cp "$B"/rpms-camera/*.rpm "$R/tmp/rpms/"
chroot "$R" rpm -Uvh --nosignature --replacepkgs /tmp/rpms/*.rpm; rm -rf "$R/tmp/rpms"
echo "== dconf, ldconfig, top menu"
chroot "$R" dconf update 2>/dev/null || true
chroot "$R" ldconfig
echo ASSEMBLE-DONE
