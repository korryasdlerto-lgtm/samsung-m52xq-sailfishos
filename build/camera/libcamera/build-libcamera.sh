#!/bin/sh
# Runs INSIDE buildroot: libcamera 0.8.0 + the m52xq patches of pmOS Phosh v8 (softISP: limit output sizes, quad Bayer, sensor info on
# configure, NV12 output, links without cached flags, AGC MSV) for Sailfish 5.2 (gcc 13.4). Kernel headers are 4.4 era: compat.
set -e
B=/root/src/libcamera-build; OUT=/root/out/libcamera
cd $B/libcamera-v0.8.0
if [ ! -e .m52xq-patched ]; then
	for p in 0001-libcamera-software_isp-Allow-limiting-output-sizes.patch 0002-libcamera-Add-quad-Bayer-CFA-layout-support.patch \
		0003-m52xq-softisp-sensor-info-on-configure-and-try-format.patch 0004-m52xq-debayer-egl-nv12-output.patch \
		0010-m52xq-simple-disable-links-without-cached-flags.patch 0011-m52xq-agc-msv-big-held-steps.patch \
		0018-m52xq-agc-msv-hold-until-settled.patch 0019-m52xq-softisp-keep-exposure-at-start.patch; do
		echo "== $p"; patch -p1 --forward --no-backup-if-mismatch < $B/patches/$p
	done
	echo "== compat"; patch -p1 --forward --no-backup-if-mismatch < $B/patches/zz-pro1x-0001-compat.patch || echo "compat not applied"
	touch .m52xq-patched
fi
rm -rf build
meson setup build --prefix=/usr --libdir=lib64 --buildtype=release -Dwerror=false -Ddocumentation=disabled -Dgstreamer=enabled \
	-Dpycamera=disabled -Dtracing=disabled -Dv4l2=true -Dcam=enabled -Dqcam=disabled -Dlc-compliance=disabled -Dtest=false \
	-Dpipelines=simple,uvcvideo -Dipas=softisp
ninja -j8 -C build
rm -rf $OUT; DESTDIR=$OUT ninja -C build install
echo BUILD-LIBCAMERA-DONE
