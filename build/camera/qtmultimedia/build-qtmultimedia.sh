#!/bin/sh
# Runs INSIDE buildroot: mlehtima/qtmultimedia v4l (c98eb6d) + the 15 pro1x patches (libcamerasrc in the camera service, rotation, flash
# LED, tap focus). Same flags as the RPM spec; only the plugins and libs are installed, into /root/out/qtmultimedia.
set -e
B=/root/src/qtmultimedia; OUT=/root/out/qtmultimedia
cd $B
export QTDIR=/usr/share/qt5
touch .git
/usr/lib64/qt5/bin/qmake GST_VERSION=1.0 QT.widgets.name= DEFINES+=QT_NO_WIDGETS -r "DEFINES+=NEMO_AUDIO" CONFIG+=config_pulseaudio
make -j8
rm -rf $OUT; make install INSTALL_ROOT=$OUT
echo BUILD-QTM-DONE
