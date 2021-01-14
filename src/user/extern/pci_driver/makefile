DIR = ../../../..
TOOLCHAIN ?= $(DIR)/i686-toolchain
SYSROOT = $(DIR)/axle-sysroot

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
LD = $(TOOLCHAIN)/bin/i686-elf-ld
LIBRARY_PATH=$(SYSROOT)/usr/i686-axle/lib:$(LIBRARY_PATH)
CFLAGS = -I$(SYSROOT)/usr/i686-axle/include -g -L$(SYSROOT)/usr/i686-axle/lib -Wl,-Bstatic -lc -nostartfiles
LDFLAGS = -T ./linker.ld -L$(SYSROOT)/usr/i686-axle/lib -lc

.ONESHELL:
pci_driver: pci_driver.c
	$(CC) $(CFLAGS) -o pci_driver.o -c pci_driver.c; \
	# Place LDFlags after object files: https://stackoverflow.com/questions/34848192/assembler-gcc-undefined-reference-to-puts
	$(LD) -o pci_driver pci_driver.o -lagx -lamc $(LDFLAGS)
	mv pci_driver $(DIR)/initrd;
