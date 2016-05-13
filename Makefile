#AS=i686-elf-as
AS2=nasm
AFLAGS=-f elf

CC=i686-elf-gcc
CFLAGS=-std=gnu99 -ffreestanding -Wall -Wextra

all: axle

checkA20.o: checkA20.s
	$(AS2) $(AFLAGS) checkA20.s

enableA20.o: enableA20.s
	$(AS2) $(AFLAGS) enableA20.s

gdt.o: gdt.s
	$(AS2) $(AFLAGS) gdt.s

interrupt.o: interrupt.s
	$(AS2) $(AFLAGS) interrupt.s

paging_util.o: paging_util.s
	$(AS2) $(AFLAGS) paging_util.s

boot.o: boot.s
	i686-elf-as -o boot.o boot.s

int32.o: int32.s
	$(AS2) $(AFLAGS) int32.s

kernel.o: kernel.c kernel.h
	$(CC) $(CFLAGS) -c kernel.c

std.o: std.c std.h
	$(CC) $(CFLAGS) -c std.c

kb.o: kb.c kb.h
	$(CC) $(CFLAGS) -c kb.c

shell.o: shell.c shell.h
	$(CC) $(CFLAGS) -c shell.c

clock.o: clock.c clock.h
	$(CC) $(CFLAGS) -c clock.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

descriptor_tables.o: descriptor_tables.c descriptor_tables.h
	$(CC) $(CFLAGS) -c descriptor_tables.c

isr.o: isr.c isr.h
	$(CC) $(CFLAGS) -c isr.c

timer.o: timer.c timer.h
	$(CC) $(CFLAGS) -c timer.c

kheap.o: kheap.c kheap.h
	$(CC) $(CFLAGS) -c kheap.c

paging.o: paging.c paging.h
	$(CC) $(CFLAGS) -c paging.c

gfx.o: gfx.c gfx.h
	$(CC) $(CFLAGS) -c gfx.c

font.o: font.c font.h
	$(CC) $(CFLAGS) -c font.c

vesa.o: vesa.c vesa.h
	$(CC) $(CFLAGS) -c vesa.c

ordered_array.o: ordered_array.c ordered_array.h
	$(CC) $(CFLAGS) -c ordered_array.c

snake.o: snake.c snake.h
	$(CC) $(CFLAGS) -c snake.c

axle: boot.o checkA20.o enableA20.o gdt.o interrupt.o paging_util.o int32.o std.o kernel.o shell.o clock.o common.o descriptor_tables.o isr.o timer.o kheap.o paging.o gfx.o font.o vesa.o snake.o ordered_array.o kb.o
	$(CC) -T linker.ld -o axle.bin -ffreestanding -nostdlib boot.o checkA20.o enableA20.o gdt.o interrupt.o paging_util.o int32.o std.o kb.o kernel.o shell.o clock.o common.o descriptor_tables.o isr.o timer.o kheap.o paging.o gfx.o font.o vesa.o ordered_array.o snake.o -lgcc

clean:
	rm *.o