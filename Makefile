# Paths
RESOURCES = resources

ISO_DIR = isodir
ISO_NAME = axle.iso

OBJ_DIR = .objs
SRC_DIR = src

TOOLCHAIN ?= ./i686-toolchain

# Compilers and flags
AS = nasm
AFLAGS = -f elf
LD = $(TOOLCHAIN)/bin/i686-elf-ld

CC = $(TOOLCHAIN)/bin/i686-elf-gcc
SYSROOT = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))axle-sysroot/
CFLAGS = -g -ffreestanding -std=gnu99 -Wall -Wextra -fstack-protector-all -I ./src
LDFLAGS = -ffreestanding -nostdlib -lgcc -T $(RESOURCES)/linker.ld

# Tools
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue
EMULATOR = qemu-system-i386
FSGENERATOR = fsgen

# Functions
findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

# Source files
PATHS = $(shell find $(SRC_DIR) -type d -print)
PATHS := $(foreach f,$(PATHS),$(if $(filter extern,$(subst /, ,$f)),,$f))
#PATHS := $(filter-out, , $(PATHS))

AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))

OBJECTS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES)))
OBJECTS := $(foreach f,$(OBJECTS),$(if $(filter extern,$(subst /, ,$f)),,$f))

INITRD = ./initrd

# Compilation flag helpers
ifdef BMP
CFLAGS += -DBMP
endif

EMFLAGS = -vga std -net nic,model=ne2k_pci -d cpu_reset -D qemu.log -serial file:syslog.log 
ifdef debug
EMFLAGS += -s -S
endif
ifdef net
EMFLAGS += -net nic,model=ne2k_pci
endif


# Rules
all: $(ISO_DIR)/boot/axle.bin

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p `dirname $@`
	$(AS) $(AFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	@mkdir -p `dirname $@`
	$(CC) $(CFLAGS) -o $@ -c $<

$(ISO_DIR)/boot/axle.bin: $(OBJECTS)
	@mkdir -p `dirname $@`
	$(CC) $(LDFLAGS) -o $@ $^

$(ISO_DIR)/boot/grub/grub.cfg: $(RESOURCES)/grub.cfg
	@mkdir -p `dirname $@`
	cp $^ $@

$(FSGENERATOR): $(FSGENERATOR).c
	@clang -o $@ $<

$(ISO_DIR)/boot/initrd.img: $(FSGENERATOR)
	@./$(FSGENERATOR) $(INITRD); mv $(INITRD).img $@

$(ISO_NAME): $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg $(ISO_DIR)/boot/initrd.img
	$(ISO_MAKER) -d ./i686-toolchain/lib/grub/i386-pc -o $@ $(ISO_DIR)

run: $(ISO_NAME)
	tmux split-window -p 75 "tail -f syslog.log"
	$(EMULATOR) $(EMFLAGS) -cdrom $^

clean:
	@rm -rf $(OBJECTS) $(ISO_DIR) $(ISO_NAME) $(FSGENERATOR)


ELFS := $(wildcard $(SRC_DIR)/user/extern/*)
TOPTARGETS := all clean
$(TOPTARGETS): $(ELFS)

.ONESHELL:
$(ELFS):
	$(AS) -f elf crt0.s -o crt0.o; \
	$(MAKE) -C $@ $(MAKECMDGOALS) SYSROOT=$(SYSROOT)
	cd initrd; \
	../fsgen .; \
	mv initrd.img ../../initrd.img; \
	cd ..; \
	cp ../initrd.img isodir/boot/;  \

.PHONY: $(TOPTARGETS) $(ELFS)

macho: macho.s
	$(AS) -f macho $< -o $@
	#ld -o $@ -e mystart -macosx_version_min 10.7 $@
	ld -o $@ -macosx_version_min 10.7 $@ mach_crt0.o
	cp $@ initrd/

