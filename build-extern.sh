#!/bin/bash

# Fail when building an external component doesn't compile
set -e

python3 ./build-libagx.py

cd ./src/user/extern/awm
make
cd ../../../../

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

cd ./src/user/extern/mouse_driver
make
cd ../../../../

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

cd ./src/user/extern/kb_driver
make
cd ../../../../

cd ./src/user/extern/tty
make
cd ../../../../

cd ./src/user/extern/window
make
cd ../../../../

cd ./src/user/extern/rainbow
make
cd ../../../../

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

rm axle.iso
make run

