TOOLCHAIN ?= ./i686-toolchain
TOOLCHAIN ?= ./i686-toolchain

AS = $(TOOLCHAIN)/bin/i686-elf-as
LD = $(TOOLCHAIN)/bin/i686-elf-ld
CC = $(TOOLCHAIN)/bin/i686-elf-gcc
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue

# turn off annoying warnings
CC_WARNING_FLAGS = -Wno-unused-parameter
CFLAGS = -ffreestanding -std=gnu99 -Wall -Wextra -I./src -O2 $(CC_WARNING_FLAGS)
LDFLAGS = -ffreestanding -nostdlib -lgcc -O2

SRC_DIR = ./src

OBJECTS = kernel.o boot.o vga_screen.o ctype.o printf.o string.o memory.o boot_info.o assert.o pmm.o

all:
	# compile step
	$(AS) -c $(SRC_DIR)/kernel/boot.s -o boot.o
	$(CC) -c $(SRC_DIR)/kernel/kernel.c -o kernel.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/kernel/boot_info.c -o boot_info.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/kernel/assert.c -o assert.o $(CFLAGS)

	$(CC) -c $(SRC_DIR)/kernel/pmm/pmm.c -o pmm.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/kernel/drivers/vga_screen/vga_screen.c -o vga_screen.o $(CFLAGS)

	$(CC) -c $(SRC_DIR)/std/ctype.c -o ctype.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/std/printf.c -o printf.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/std/string.c -o string.o $(CFLAGS)
	$(CC) -c $(SRC_DIR)/std/memory.c -o memory.o $(CFLAGS)
	
	# link step
	$(CC) -T link.ld -o axle.bin $(LDFLAGS) $(OBJECTS)
	
	cp axle.bin isodir/boot/axle.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	$(ISO_MAKER) -d $(TOOLCHAIN)/lib/grub/i386-pc -o axle.iso isodir

run:
	qemu-system-x86_64 -monitor stdio -cdrom axle.iso
