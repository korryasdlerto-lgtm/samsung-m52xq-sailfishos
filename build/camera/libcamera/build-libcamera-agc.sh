#!/bin/sh
# Runs INSIDE buildroot: incremental build in the existing libcamera tree (same IPA signing key), exposure loop patch 0018.
set -e
B=/root/src/libcamera-build; OUT=/root/out/libcamera-agc
cd $B/libcamera-v0.8.0
if [ ! -e .m52xq-0018 ]; then
	cp -p src/ipa/libipa/agc_msv.cpp src/ipa/libipa/agc_msv.h /root/out/ 2>/dev/null || true
	patch -p1 --forward --no-backup-if-mismatch < $B/patches/0018-m52xq-agc-msv-hold-until-settled.patch
	touch .m52xq-0018
fi
if [ ! -e .m52xq-0019 ]; then
	patch -p1 --forward --no-backup-if-mismatch < $B/patches/0019-m52xq-softisp-keep-exposure-at-start.patch
	touch .m52xq-0019
fi
K=build/src/ipa-priv-key.pem; md5sum $K
ninja -j8 -C build
rm -rf $OUT; DESTDIR=$OUT ninja -C build install > /dev/null
md5sum $K
openssl pkey -in $K -pubout -out /tmp/ipa-pub.pem
openssl dgst -sha256 -verify /tmp/ipa-pub.pem -signature $OUT/usr/lib64/libcamera/ipa/ipa_softisp.so.sign $OUT/usr/lib64/libcamera/ipa/ipa_softisp.so
md5sum $OUT/usr/lib64/libcamera/ipa/* $OUT/usr/lib64/libcamera.so.0.8.0
echo BUILD-LIBCAMERA-AGC-DONE
