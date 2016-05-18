#!/bin/bash
make
cp axle.bin isodir/boot/axle.bin
cp src/boot/grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue --xorriso=/Volumes/Files/Developer/opt/cross/bin/i686-elf-xorriso -o axle.iso isodir

qemu-system-i386 -vga std -cdrom axle.iso
