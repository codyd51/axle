#!/bin/bash

# Fail when building an external component doesn't compile
set -e

# Copy headers that are used externally to the sysroot
cp src/kernel/util/amc/amc.h axle-sysroot/usr/i686-axle/include/kernel/amc.h
cp src/kernel/util/adi/adi.h axle-sysroot/usr/i686-axle/include/kernel/adi.h
cp src/kernel/interrupts/idt.h axle-sysroot/usr/i686-axle/include/kernel/idt.h

# Copy awm headers to the sysroot so other programs can use its message protocol
cp src/user/extern/awm/awm.h axle-sysroot/usr/i686-axle/include/awm/awm.h
cp src/user/extern/awm/awm_messages.h axle-sysroot/usr/i686-axle/include/awm/awm_messages.h
cp src/user/extern/pci_driver/pci_messages.h axle-sysroot/usr/i686-axle/include/pci/pci_messages.h
cp src/user/extern/net/net_messages.h axle-sysroot/usr/i686-axle/include/net/net_messages.h
cp src/user/extern/realtek_8139_driver/rtl8139_messages.h axle-sysroot/usr/i686-axle/include/drivers/realtek_8139/rtl8139_messages.h
cp src/user/extern/watchdogd/watchdogd_messages.h axle-sysroot/usr/i686-axle/include/daemons/watchdogd/watchdogd_messages.h
cp src/user/extern/preferences/preferences_messages.h axle-sysroot/usr/i686-axle/include/preferences/preferences_messages.h
cp src/user/extern/kb_driver/kb_driver_messages.h axle-sysroot/usr/i686-axle/include/kb_driver/kb_driver_messages.h
cp src/user/extern/file_manager/file_manager_messages.h axle-sysroot/usr/i686-axle/include/file_manager/file_manager_messages.h
cp src/user/extern/image_viewer/image_viewer_messages.h axle-sysroot/usr/i686-axle/include/image_viewer/image_viewer_messages.h

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

cd ./src/user/extern/awm
make
cd ../../../../

cd ./src/user/extern/mouse_driver
make
cd ../../../../

cd ./src/user/extern/kb_driver
make
cd ../../../../

cd ./src/user/extern/realtek_8139_driver
make
cd ../../../../

cd ./src/user/extern/tty
make
cd ../../../../

cd ./src/user/extern/pci_driver
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

cd ./src/user/extern/net
make
cd ../../../../

cd ./src/user/extern/netclient
make
cd ../../../../

cd ./src/user/extern/watchdogd
make
cd ../../../../

cd ./src/user/extern/tlsclient
make
cd ../../../../

cd ./src/user/extern/preferences
make
cd ../../../../

cd ./src/user/extern/2048
make
cd ../../../../

cd ./src/user/extern/breakout
make
cd ../../../../

cd ./src/user/extern/snake
make
cd ../../../../

cd ./src/user/extern/file_manager
make
cd ../../../../

cd ./src/user/extern/image_viewer
make
cd ../../../../

cd ./src/user/extern/activity_monitor
make
cd ../../../../

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

sudo rm axle.iso
make run
