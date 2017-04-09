DIR = ../../../../
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = /Users/PhillipTennen/axle/

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
CFLAGS = -I$(SYSROOT)/usr/include -g -L$(SYSROOT)/usr/lib -Wl,-Bstatic -lc -nostartfiles

.ONESHELL:
shell: shell.c lib/iberty/iberty.c
	$(CC) $(CFLAGS) $(DIR)/crt0.o shell.c lib/iberty/iberty.c -o ash; \
	mv ash $(DIR)/initrd;
