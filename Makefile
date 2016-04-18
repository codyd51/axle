AS=i686-elf-as
CC=i686-elf-gcc

axle: boot.s kernel.c shell.c linker.ld
	$(AS) boot.s -o boot.o
	$(CC) -c shell.c -o shell.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -T linker.ld -o axle.bin -ffreestanding -O2 -nostdlib boot.o kernel.o shell.o -lgcc
