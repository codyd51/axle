DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

tlsclient: tlsclient.c tls.c asn1.c big_int.c $(SYSROOT)/usr/i686-axle/lib/libgui.a $(SYSROOT)/usr/i686-axle/lib/libagx.a
	$(CC) $(CFLAGS) -o tlsclient.o -c tlsclient.c; \
	$(CC) $(CFLAGS) -o tls.o -c tls.c; \
	$(CC) $(CFLAGS) -o asn1.o -c asn1.c; \
	$(CC) $(CFLAGS) -o big_int.o -c big_int.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o tlsclient tlsclient.o tls.o asn1.o big_int.o -lgui -lnet -lamc -lagx -lport $(LDFLAGS); \
	cp tlsclient $(DIR)/initrd;
