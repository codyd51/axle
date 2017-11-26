DIR = ../../../../
TOOLCHAIN ?= $(DIR)/i686-toolchain

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
CFLAGS = -I$(SYSROOT)/usr/include -g -L$(SYSROOT)/usr/lib -Wl,-Bstatic -lc -nostartfiles

.ONESHELL:
true: true.c
	$(CC) $(CFLAGS) $(DIR)/crt0.o true.c -o true; \
	mv true $(DIR)/initrd;
