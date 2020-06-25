#!/bin/bash
cd ./src/user/extern/print_and_exit
make
cd ../../../../

cd ./src/user/extern/cat
make
cd ../../../../

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

