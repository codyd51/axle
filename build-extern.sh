#!/bin/bash

# Fail when building an external component doesn't compile
set -e

# Copy headers that are used externally to the sysroot
cp src/kernel/util/amc/amc.h axle-sysroot/usr/i686-axle/include/kernel/amc.h
cp src/kernel/util/adi/adi.h axle-sysroot/usr/i686-axle/include/kernel/adi.h
cp src/kernel/interrupts/idt.h axle-sysroot/usr/i686-axle/include/kernel/idt.h

# Copy the kernel bits of the the sysroot's include/ to the newlib port
# This is so syscalls get the right struct definitions
cp -r axle-sysroot/usr/i686-axle/include/kernel ports/newlib/newlib-2.5.0.20171222/newlib/libc/sys/axle/include/

# Copy awm headers to the sysroot so other programs can use its message protocol
cp src/user/extern/awm/awm.h axle-sysroot/usr/i686-axle/include/awm/awm.h

python3 ./build-libs.py

cd ./src/user/extern/awm
make
cd ../../../../

cd ./src/user/extern/mouse_driver
make
cd ../../../../

cd ./src/user/extern/kb_driver
make
cd ../../../../

cd ./src/user/extern/tty
make
cd ../../../../

cd ./src/user/extern/rainbow
make
cd ../../../../

cd ./src/user/extern/paintbrush
make
cd ../../../../

cd ./src/user/extern/textpad
make
cd ../../../../

cd ./src/user/extern/pci_driver
make
cd ../../../../

cd ./src/user/extern/realtek_8139_driver
make
cd ../../../../

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img


rm axle.iso
make run

exit


cd ./src/user/extern/print_and_exit
make
cd ../../../../

cd ./src/user/extern/cat
make
cd ../../../../

cd ./src/user/extern/postman
make
cd ../../../../

cd ./src/user/extern/window
make
cd ../../../../


cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

rm axle.iso
make run

