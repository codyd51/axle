DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

.ONESHELL:
awm: awm.c font/font.c lib/point.c lib/color.c lib/putpixel.c lib/size.c lib/ca_layer.c lib/shapes.c lib/rect.c
	$(CC) $(CFLAGS) -o awm.o -c awm.c; \
	$(CC) $(CFLAGS) -o font.o -c font/font.c; \
	$(CC) $(CFLAGS) -o point.o -c lib/point.c; \
	$(CC) $(CFLAGS) -o color.o -c lib/color.c; \
	$(CC) $(CFLAGS) -o putpixel.o -c lib/putpixel.c; \
	$(CC) $(CFLAGS) -o size.o -c lib/size.c; \
	$(CC) $(CFLAGS) -o ca_layer.o -c lib/ca_layer.c; \
	$(CC) $(CFLAGS) -o shapes.o -c lib/shapes.c; \
	$(CC) $(CFLAGS) -o rect.o -c lib/rect.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o awm awm.o font.o point.o color.o putpixel.o size.o ca_layer.o shapes.o rect.o $(LDFLAGS)
	mv awm $(DIR)/initrd;
