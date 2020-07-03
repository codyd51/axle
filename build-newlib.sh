#!/bin/bash
ln ./i686-toolchain/bin/i686-elf-ar i686-axle-ar
ln ./i686-toolchain/bin/i686-elf-as i686-axle-as
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-gcc
ln ./i686-toolchain/bin/i686-elf-gcc i686-axle-cc
ln ./i686-toolchain/bin/i686-elf-ranlib i686-axle-ranlib
export PATH=./i686-toolchain/bin/:$PATH
export PATH=/Users/philliptennen/Documents/develop/axle/newlib/newlib_bin/bin/:$PATH
export PATH=~/Documents/develop/axle/i686-toolchain/bin/:$PATH
cd "./newlib/build-newlib"
pwd
make all
make DESTDIR=~/Documents/develop/axle/axle-sysroot-2020 install
