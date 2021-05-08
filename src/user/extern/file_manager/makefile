DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

file_manager: file_manager.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libgui.a
	$(CC) $(CFLAGS) -o file_manager.o -c file_manager.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o file_manager file_manager.o -lgui -lagx -lamc $(LDFLAGS)
	cp file_manager $(DIR)/initrd;
