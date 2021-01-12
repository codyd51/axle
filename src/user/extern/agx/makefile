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
# build-libagx.py script.
agx: font/font.c lib/point.c lib/color.c lib/putpixel.c lib/size.c lib/ca_layer.c lib/shapes.c lib/rect.c lib/text_box.c
	$(CC) $(CFLAGS) -o font.o -c font/font.c; 
	$(CC) $(CFLAGS) -o point.o -c lib/point.c; 
	$(CC) $(CFLAGS) -o color.o -c lib/color.c; 
	$(CC) $(CFLAGS) -o putpixel.o -c lib/putpixel.c; 
	$(CC) $(CFLAGS) -o size.o -c lib/size.c; 
	$(CC) $(CFLAGS) -o ca_layer.o -c lib/ca_layer.c; 
	$(CC) $(CFLAGS) -o shapes.o -c lib/shapes.c; 
	$(CC) $(CFLAGS) -o rect.o -c lib/rect.c; 
	$(CC) $(CFLAGS) -o text_box.o -c lib/text_box.c; 
	$(AR) rcs libagx.a font.o point.o color.o putpixel.o size.o ca_layer.o shapes.o rect.o text_box.o
	$(RANLIB) libagx.a
