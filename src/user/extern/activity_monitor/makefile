DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

activity_monitor: activity_monitor.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libgui.a
	$(CC) $(CFLAGS) -o activity_monitor.o -c activity_monitor.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o activity_monitor activity_monitor.o -lgui -lagx -lamc $(LDFLAGS)
	cp activity_monitor $(DIR)/initrd;
