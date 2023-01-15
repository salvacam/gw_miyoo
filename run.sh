#!/bin/sh
export PATH=$PATH:/opt/gcw0-toolchain/usr/bin
export TARGETMACH=mipsel-linux
export BUILDMACH=i686-pc-linux-gnu
export CROSS=mipsel-linux
export CC=${CROSS}-gcc
export LD=${CROSS}-ld
export AS=${CROSS}-as
export CXX=${CROSS}-g++
make clean
make -j4
rm -rf gw-libretro.opk
mksquashfs opk/* gw_libretro.so lr/lr gw-libretro.opk -all-root -noappend -no-exports -no-xattrs -no-progress >/dev/null
