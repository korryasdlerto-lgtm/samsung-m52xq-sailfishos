#!/bin/sh
# Runs INSIDE buildroot: apply 0023 (DSDS SIM 2) on the already patched ofono tree, incremental make, install to /root/out/ofono-0023
set -e
B=/root/src/ofono-build; OUT=/root/out/ofono-0023
cd $B/ofono
patch -p1 --forward --no-backup-if-mismatch < $B/0023-m52xq-dsds-sim2.patch
make -j16 > ../ofono-make-0023.log 2>&1 || { tail -40 ../ofono-make-0023.log; exit 1; }
rm -rf $OUT; make install DESTDIR=$OUT > /dev/null
ls -l $OUT/usr/sbin/ofonod
echo BUILD-0023-DONE
