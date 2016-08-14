#!/bin/bash

cd ./initrd
../fsgen .
cd ..
mv initrd/initrd.img isodir/boot/initrd.img
