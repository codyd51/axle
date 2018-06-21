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
CFLAGS = -g -ffreestanding -std=gnu99 -Wall -Wextra -I./src
LDFLAGS = -ffreestanding -nostdlib -lgcc -T $(RESOURCES)/linker.ld

# Tools
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue
EMULATOR = qemu-system-i386
FSGENERATOR = fsgen

GDB = $(TOOLCHAIN)/bin/i686-elf-gdb
GDB_FLAGS = -x script.gdb

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

EMFLAGS = -vga std -net nic,model=ne2k_pci -D qemu.log -serial file:syslog.log -monitor stdio -d guest_errors
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

$(ISO_NAME): $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg 
	$(ISO_MAKER) -d ./i686-toolchain/lib/grub/i386-pc -o $@ $(ISO_DIR)

run: $(ISO_NAME)
	$(EMULATOR) $(EMFLAGS) -cdrom $^

dbg:
	$(GDB) $(GDB_FLAGS)

clean:
	@rm -rf $(OBJECTS) $(ISO_DIR) $(ISO_NAME) $(FSGENERATOR)

