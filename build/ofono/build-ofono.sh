#!/bin/sh
# Runs INSIDE buildroot: ell 0.70 (kernel.org) + ofono 2.12 (sailfishos-native/ofono 4c79652, upstream submodule 2.12) with the
# Sailfish rpm patches + moWerk pro1x 0019-0022 (qrtrqmi slot driver, IPA data path, call notifications, APN/IPv6).
set -e
B=/root/src/ofono-build; OUT=/root/out; rm -rf $OUT/ell $OUT/ofono
cd $B && rm -rf ell-0.70 && tar -xJf ell-0.70.tar.xz && cd ell-0.70
./configure --prefix=/usr --libdir=/usr/lib64 > ../ell-configure.log 2>&1
make -j16 > ../ell-make.log 2>&1
make install DESTDIR=$OUT/ell > /dev/null; make install > /dev/null
echo "ell OK"
cd $B/ofono
git apply --check $B/$(head -1 $B/series) 2>/dev/null || true
for p in $(cat $B/series); do echo "apply $p"; patch -p1 --forward --no-backup-if-mismatch < $B/$p > /dev/null || { echo "PATCH FAIL $p"; exit 1; }; done
autoreconf --force --install > ../autoreconf.log 2>&1
./configure --prefix=/usr --libdir=/usr/lib64 --sysconfdir=/etc --localstatedir=/var --disable-static --enable-test \
  --enable-sailfish-slot --enable-sailfish-bt --enable-sailfish-pushforwarder --enable-sailfish-access --disable-rilmodem \
  --disable-isimodem --enable-qmimodem --with-systemdunitdir=/usr/lib/systemd/system --enable-external-ell --enable-debug=yes > ../ofono-configure.log 2>&1
make -j16 > ../ofono-make.log 2>&1
make install DESTDIR=$OUT/ofono > /dev/null
find $OUT/ofono -type f | grep -vE '/test/|/include/' | head -20
echo BUILD-OFONO-DONE
