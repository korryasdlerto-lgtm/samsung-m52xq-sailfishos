#!/bin/sh
set -e
cd /root/src/rawphoto; export PKG_CONFIG_PATH=/usr/lib64/pkgconfig
gcc -O2 -Wall -D_GNU_SOURCE -I. -c rawphoto.c photo_gpu.c
g++ -O2 -Wall -std=c++20 -D_GNU_SOURCE -I. $(pkg-config --cflags libcamera) -c m52xq-rawphoto.cpp
g++ -o m52xq-rawphoto m52xq-rawphoto.o rawphoto.o photo_gpu.o -lcamera -lcamera-base -lEGL -lGLESv2 -ljpeg -lm -lpthread
strip m52xq-rawphoto; ls -l m52xq-rawphoto; ldd m52xq-rawphoto | grep "not found" || true
echo RAWPHOTO-DONE
