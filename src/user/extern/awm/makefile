DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

awm: awm.c effects.c utils.c window.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libimg.a
	$(CC) $(CFLAGS) -o awm.o -c awm.c; 
	$(CC) $(CFLAGS) -o effects.o -c effects.c; 
	$(CC) $(CFLAGS) -o window.o -c window.c; 
	$(CC) $(CFLAGS) -o utils.o -c utils.c; 
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o awm awm.o effects.o window.o utils.o -lagx -lamc -limg $(LDFLAGS)
	cp awm $(DIR)/initrd;
