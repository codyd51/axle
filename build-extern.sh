#!/bin/bash

# Fail when building an external component doesn't compile
set -e

# TODO(PT): Move these copies to build_all.py
# Copy the kernel bits of the the sysroot's include/ to the newlib port
# This is so syscalls get the right struct definitions
cp -r axle-sysroot/usr/i686-axle/include/kernel ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/include/
# And the needed message definitions
cp -r axle-sysroot/usr/i686-axle/include/daemons ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/include/

# Copy stdlib additions to the sysroot
cp ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/array.h axle-sysroot/usr/i686-axle/include/stdlibadd/array.h
cp ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/assert.h axle-sysroot/usr/i686-axle/include/stdlibadd/assert.h
cp ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/sleep.h axle-sysroot/usr/i686-axle/include/stdlibadd/sleep.h

python3 ./build-libs.py

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

sudo rm axle.iso
make run
