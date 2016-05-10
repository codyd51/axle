AS=i686-elf-as
CC=i686-elf-gcc



axle: boot.s linker.ld std.c kb.c kernel.c shell.c clock.c common.c ide.c enableA20.s descriptor_tables.h descriptor_tables.c gdt.s interrupt.s isr.c isr.h timer.h timer.c kheap.h kheap.c paging.h paging.c paging_util.s int32.s gfx.h gfx.c font.h font.c vesa.h vesa.c
	$(AS) boot.s -o boot.o
	nasm -f elf -o checkA20.o checkA20.s
	nasm -f elf -o enableA20.o enableA20.s
	nasm -f elf -o gdt.o gdt.s
	nasm -f elf -o interrupt.o interrupt.s
	nasm -f elf -o paging_util.o paging_util.s
	nasm -f elf -o int32.o int32.s
	$(CC) -c std.c -o std.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c kb.c -o kb.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c shell.c -o shell.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c clock.c -o clock.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	$(CC) -c common.c -o common.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c ide.c -o ide.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c descriptor_tables.c -o descriptor_tables.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c isr.c -o isr.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c timer.c -o timer.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c kheap.c -o kheap.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c paging.c -o paging.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c gfx.c -o gfx.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c font.c -o font.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -c vesa.c -o vesa.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(CC) -T linker.ld -o axle.bin -ffreestanding -O2 -nostdlib boot.o checkA20.o enableA20.o kernel.o shell.o kb.o std.o clock.o common.o gdt.o interrupt.o descriptor_tables.o isr.o timer.o kheap.o paging.o paging_util.o int32.o gfx.o font.o vesa.o -lgcc
