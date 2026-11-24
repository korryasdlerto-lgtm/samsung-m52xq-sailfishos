#!/bin/sh
# Runs INSIDE buildroot: only the camerabin plugin of QtMultimedia
set -e
cd /root/src/qtmultimedia/src/plugins/gstreamer/camerabin; export QTDIR=/usr/share/qt5
make -j8
L=$(find /root/src/qtmultimedia/plugins /root/src/qtmultimedia/src/plugins -name libgstcamerabin.so | head -1); mkdir -p /root/out/camerabin; cp $L /root/out/camerabin/; strip --strip-unneeded /root/out/camerabin/libgstcamerabin.so; md5sum /root/out/camerabin/libgstcamerabin.so; echo CB-DONE
