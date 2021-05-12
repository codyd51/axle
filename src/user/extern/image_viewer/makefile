DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

image_viewer: image_viewer.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libgui.a  $(SYSROOT)/usr/i686-axle/lib/libimg.a
	$(CC) $(CFLAGS) -o image_viewer.o -c image_viewer.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o image_viewer image_viewer.o -lgui -lagx -lamc -limg $(LDFLAGS)
	cp image_viewer $(DIR)/initrd;
