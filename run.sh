#!/bin/bash
set -e

mkdir -p isodir/boot/grub/
make
cp axle.bin isodir/boot/axle.bin
cp src/boot/grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue -o axle.iso isodir

qemu-system-i386 -vga std -cdrom axle.iso
