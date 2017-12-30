TOOLCHAIN ?= ./i686-toolchain
TOOLCHAIN ?= ./i686-toolchain

AS = $(TOOLCHAIN)/bin/i686-elf-as
LD = $(TOOLCHAIN)/bin/i686-elf-ld
CC = $(TOOLCHAIN)/bin/i686-elf-gcc
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue

CFLAGS = -ffreestanding -std=gnu99 -Wall -Wextra -O2
LDFLAGS = -ffreestanding -nostdlib -O2 -lgcc

SRC_DIR = ./src

OBJECTS = kernel.o boot.o vga_screen.o

all:
	# compile step
	$(AS) $(SRC_DIR)/kernel/boot.s -o boot.o
	$(CC) -c $(SRC_DIR)/kernel/kernel.c -o kernel.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/kernel/drivers/vga_screen/vga_screen.c -o vga_screen.o $(CFLAGS)
	# link step
	$(CC) -T link.ld -o tremble.bin $(LDFLAGS) $(OBJECTS)
	
	cp tremble.bin isodir/boot/tremble.bin
	cp grub.cfg isodir/boot/grub.cfg
	$(ISO_MAKER) -d ./i686-toolchain/lib/grub/i386-pc -o tremble.iso isodir

run:
	qemu-system-i386 -cdrom tremble.iso
