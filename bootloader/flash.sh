#!/bin/sh

set -e

cd uefi
make
cd ..

make

cd kernel
make
cd ..

dd if=/dev/zero of=fat.img bs=512 count=131072
# mformat -i fat.img -f 92160 ::
disk=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount ./fat.img)
echo $disk
newfs_msdos -F 32 -S 512 -s 131072 $disk
mmd -i fat.img ::/EFI
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img BOOTX64.EFI ::/EFI/BOOT

# ~/axle.nosync/x86_64-toolchain/bin/x86_64-elf-ar -frsv libuefi.a dirent.o qsort.o stat.o stdio.o stdlib.o string.o time.o unistd.o
#                                               ar -frsv libuefi.a dirent.o qsort.o stat.o stdio.o stdlib.o string.o time.o unistd.o

mmd -i fat.img ::/EFI/AXLE
mcopy -i fat.img kernel/kernel.elf ::/EFI/AXLE/KERNEL.ELF
mcopy -i fat.img initrd.img ::/EFI/AXLE/INITRD.IMG

# umount $disk

qemu-system-x86_64 -bios /Users/philliptennen/Downloads/RELEASEX64_OVMF.fd -usb -drive if=none,id=stick,format=raw,file=./fat.img -device usb-storage,drive=stick -monitor stdio -m 2G -serial file:syslog.log
