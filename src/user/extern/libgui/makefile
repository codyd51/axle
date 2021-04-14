DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
AR = $(TOOLCHAIN)/bin/i686-elf-ar
RANLIB = $(TOOLCHAIN)/bin/i686-elf-ranlib
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles

# The resulting static library will be moved into the sysroot by the 
# build-libs.py script.
libgui.a: libgui.c $(SYSROOT)/usr/i686-axle/lib/libagx.a $(SYSROOT)/usr/i686-axle/lib/libamc.a
	$(CC) $(CFLAGS) -o libgui.o -c libgui.c; \
	$(AR) rcs libgui.a libgui.o; \
	$(RANLIB) libgui.a;
