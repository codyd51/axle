#!/bin/bash

# Fail if something doesn't work
set -e

cd initrd
../fsgen ./
mv initrd.img ../initrd.img
cd ..
mv initrd.img isodir/boot/initrd.img

# sudo rm axle.iso
rm axle.iso
make run
