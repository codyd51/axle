DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
AR = $(TOOLCHAIN)/bin/i686-elf-ar
RANLIB = $(TOOLCHAIN)/bin/i686-elf-ranlib
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles

# The resulting static library will be copied into the sysroot by the 
# build-libs.py script.
libport.a: libport.c
	$(CC) $(CFLAGS) -o libport.o -c libport.c;
	$(AR) rcs libport.a libport.o
	$(RANLIB) libport.a
