#!/bin/sh
# Runs INSIDE buildroot: apply 0029 (SIM 2 own mux) on the already patched ofono tree, incremental make, install to /root/out/ofono-0029
set -e
B=/root/src/ofono-build; OUT=/root/out/ofono-0029
cd $B/ofono
patch -p1 --forward --no-backup-if-mismatch < $B/0029-m52xq-qrtrqmi-sim2-own-mux.patch
make -j8 > ../ofono-make-0029.log 2>&1 || { tail -40 ../ofono-make-0029.log; exit 1; }
rm -rf $OUT; make install DESTDIR=$OUT > /dev/null
ls -l $OUT/usr/sbin/ofonod
echo BUILD-0029-DONE
