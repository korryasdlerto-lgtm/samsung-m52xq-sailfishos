#!/bin/sh
# Runs INSIDE buildroot: sensorfw-qt5-ssc (sensord adaptors for the SSC, from moWerk/sailfishos-pro1x-mainline 9e815b5,
# packages/sensorfw-qt5-ssc) against our libssc 0.4.4 (m52xq patch 0002); the step counter adaptor is left out (it needs the
# pro1x libssc pedometer patch, our 0003 is the proximity dump).
set -e
cd /root/src/sensorfw-qt5-ssc
Q=$(ls /usr/lib64/qt5/bin/qmake /usr/bin/qmake-qt5 /usr/bin/qmake 2>/dev/null | head -1); echo "qmake: $Q"
make distclean >/dev/null 2>&1 || true
$Q sensorfw-qt5-ssc.pro
make -j16
rm -rf /root/out/sensorfw-qt5-ssc; make install INSTALL_ROOT=/root/out/sensorfw-qt5-ssc
find /root/out/sensorfw-qt5-ssc -type f
echo BUILD-SENSORS-2-DONE
