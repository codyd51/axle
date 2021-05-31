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
libimg.a: libimg.c nanojpeg.c
	$(CC) $(CFLAGS) -o libimg.o -c libimg.c;
	$(CC) $(CFLAGS) -o nanojpeg.o -c nanojpeg.c;
	$(AR) rcs libimg.a libimg.o nanojpeg.o
	$(RANLIB) libimg.a
