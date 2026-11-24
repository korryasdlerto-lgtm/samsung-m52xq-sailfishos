#!/bin/sh
# Runs INSIDE buildroot: only libqgsttools_p of QtMultimedia
set -e
cd /root/src/qtmultimedia/src/gsttools; export QTDIR=/usr/share/qt5
make -j8
L=$(ls -t /root/src/qtmultimedia/lib/libqgsttools_p.so.1.0.0); mkdir -p /root/out/qgsttools; cp $L /root/out/qgsttools/; strip --strip-unneeded /root/out/qgsttools/libqgsttools_p.so.1.0.0
readelf -d /root/out/qgsttools/libqgsttools_p.so.1.0.0 | grep -E "SONAME|RPATH|RUNPATH"; md5sum /root/out/qgsttools/*; ls -la /root/out/qgsttools; echo QGST-DONE
