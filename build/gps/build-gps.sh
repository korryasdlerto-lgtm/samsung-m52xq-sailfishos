#!/bin/sh
# Runs INSIDE buildroot: geoclue-provider-hybris-qmi (moWerk pro1x 9e815b5: mer-hybris/geoclue-providers-hybris b126f58 + qmi/ QMI LOC
# over QRTR backend), qmake in qmi/ like the spec (%qmake_command), install to /root/out/gps.
set -e
cd /root/src/geoclue-qmi && cp qmi/* . && sed -i "s#\$\$PWD/../geoclue-providers-hybris.pri#\$\$PWD/geoclue-providers-hybris.pri#" qmilocationbackend.pro
Q=/usr/lib64/qt5/bin/qmake
make distclean >/dev/null 2>&1 || true
$Q qmilocationbackend.pro
make -j16
rm -rf /root/out/gps; make install INSTALL_ROOT=/root/out/gps
find /root/out/gps -type f
echo BUILD-GPS-DONE
