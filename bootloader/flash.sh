#!/bin/sh

set -e

cd uefi
make
cd ..

make

dd if=/dev/zero of=fat.img bs=512 count=2880
mformat -i fat.img -f 2880 ::
mmd -i fat.img ::/EFI
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img BOOTX64.EFI ::/EFI/BOOT

mmd -i fat.img ::/EFI/AXLE
mcopy -i fat.img kernel/kernel.elf ::/EFI/AXLE/KERNEL.ELF

qemu-system-x86_64 -bios /home/phillip/Downloads/OVMF-pure-efi.fd -usb -drive if=none,id=stick,format=raw,file=./fat.img -device usb-storage,drive=stick -monitor stdio -m 2G
