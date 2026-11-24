#!/bin/sh
# Runs INSIDE buildroot (copy of the Sailfish 5.2.0.22 rootfs + gcc/meson/qt5-devel). protobuf-c 1.5.2, then libssc 0.4.4 with the
# m52xq patches like debian-phosh/build/scripts/build-libssc.sh: system library with 0002, /usr/lib64/m52xq/libssc.so.2 with 0003.
set -e
export LC_ALL=C
OUT=/root/out; rm -rf $OUT; mkdir -p $OUT/protobuf-c $OUT/libssc
cd /root/src
rm -rf protobuf-c-1.5.2 && tar -xzf protobuf-c-1.5.2.tar.gz && cd protobuf-c-1.5.2
./configure --prefix=/usr --libdir=/usr/lib64 > ../protobuf-c-configure.log 2>&1
make -j16 > ../protobuf-c-make.log 2>&1
make install DESTDIR=$OUT/protobuf-c > /dev/null
make install > /dev/null            # also into the build root, libssc needs it
echo "protobuf-c OK: $(ls /usr/lib64/libprotobuf-c.so.* | tr '\n' ' ') $(which protoc-c protoc-gen-c 2>/dev/null)"
cd /root/src
for v in 0002 0003; do
  rm -rf libssc-src-$v libssc-dest-$v && mkdir libssc-src-$v libssc-dest-$v
  tar -xzf libssc-0.4.4.tar.gz -C libssc-src-$v --strip-components=1
  ( cd libssc-src-$v && patch -p1 < /root/src/$(ls /root/src | grep "libssc-m52xq-$v-") )
  ( cd libssc-src-$v && meson setup build --prefix=/usr --libdir=lib64 --buildtype=release > /root/src/libssc-$v-meson.log 2>&1 \
     && meson compile -C build >> /root/src/libssc-$v-meson.log 2>&1 && DESTDIR=/root/src/libssc-dest-$v meson install -C build >> /root/src/libssc-$v-meson.log 2>&1 )
done
cp -a libssc-dest-0002/. $OUT/libssc/
mkdir -p $OUT/libssc/usr/lib64/m52xq
cp -a $(find libssc-dest-0003 -name 'libssc.so.2*' -type f | sort | tail -1) $OUT/libssc/usr/lib64/m52xq/libssc.so.2
cp -a libssc-dest-0002/. /            # into the build root for sensorfw-qt5-ssc
find $OUT -type f | sed "s|$OUT/||" | grep -vE '/include/|\.pc$|\.a$|\.la$|share/doc' | sort
echo BUILD-SENSORS-1-DONE
