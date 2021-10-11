#!/bin/bash
ln ./i686-toolchain/bin/i686-elf-ar i686-axle-ar
ln ./i686-toolchain/bin/i686-elf-as i686-axle-as
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-gcc
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-cc
ln ./i686-toolchain/bin/i686-elf-ranlib i686-axle-ranlib
export PATH=/Users/philliptennen/Documents/develop/axle.nosync/i686-toolchain/bin/:$PATH
export PATH=/Users/philliptennen/Documents/develop/axle.nosync/ports/newlib/bin/bin/:$PATH
cd "./ports/newlib/build-newlib"
pwd

# If you make some kind of config change to the axle target, such as adding new files within the newlib port,
# you may have to run this command
# You may see an error like the following while running this script:
# /bin/sh: /Users/philliptennen/Documents/develop/axle/ports/newlib/newlib-2.5.0.20171222/etc/configure: No such file or directory
# ../newlib-2.5.0.20171222/configure --prefix=/usr --target=i686-axle

# Fail when newlib doesn't compile
set -e

make all
make DESTDIR=~/Documents/develop/axle.nosync/axle-sysroot install
