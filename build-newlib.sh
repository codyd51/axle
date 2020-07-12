#!/bin/bash
ln ./i686-toolchain/bin/i686-elf-ar i686-axle-ar
ln ./i686-toolchain/bin/i686-elf-as i686-axle-as
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-gcc
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-cc
ln ./i686-toolchain/bin/i686-elf-ranlib i686-axle-ranlib
export PATH=./i686-toolchain/bin/:$PATH
export PATH=/Users/philliptennen/Documents/develop/axle/ports/newlib/bin/bin/:$PATH
cd "./ports/newlib/build-newlib"
pwd
# ../newlib-2.5.0.20171222/configure --prefix=/usr --target=i686-axle
make all
make DESTDIR=~/Documents/develop/axle/axle-sysroot install
