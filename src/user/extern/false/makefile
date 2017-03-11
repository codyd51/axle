DIR = ../../../../
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = /Users/PhillipTennen/axle/

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
CFLAGS = -I$(SYSROOT)/usr/include -g -L$(SYSROOT)/usr/lib -Wl,-Bstatic -lc -nostartfiles

.ONESHELL:
false: false.c
	$(CC) $(CFLAGS) $(DIR)/crt0.o false.c -o false; \
	mv false $(DIR)/initrd;
