DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

.ONESHELL:
cat: cat.c
	$(CC) $(CFLAGS) -o cat.o -c cat.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o cat cat.o $(LDFLAGS)
	mv cat $(DIR)/initrd;
