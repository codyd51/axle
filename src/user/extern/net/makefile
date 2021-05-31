DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles -fstack-protector-all
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

.ONESHELL:
net: net.c util.c ethernet.c arp.c ipv4.c udp.c dns.c tcp.c callback.c $(SYSROOT)/usr/i686-axle/lib/libgui.a
	$(CC) $(CFLAGS) -o net.o -c net.c; \
	$(CC) $(CFLAGS) -o util.o -c util.c; \
	$(CC) $(CFLAGS) -o ethernet.o -c ethernet.c; \
	$(CC) $(CFLAGS) -o arp.o -c arp.c; \
	$(CC) $(CFLAGS) -o ipv4.o -c ipv4.c; \
	$(CC) $(CFLAGS) -o udp.o -c udp.c; \
	$(CC) $(CFLAGS) -o dns.o -c dns.c; \
	$(CC) $(CFLAGS) -o tcp.o -c tcp.c; \
	$(CC) $(CFLAGS) -o callback.o -c callback.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o net net.o util.o ethernet.o arp.o ipv4.o udp.o dns.o tcp.o callback.o -lgui -lagx -lamc -lport $(LDFLAGS)
	cp net $(DIR)/initrd;
