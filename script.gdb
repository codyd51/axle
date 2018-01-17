symbol-file isodir/boot/axle.bin
target remote | qemu-system-i386 -vga std -net nic,model=ne2k_pci -d cpu_reset -D qemu.log -serial file:syslog.log -cdrom axle.iso -S -gdb stdio
