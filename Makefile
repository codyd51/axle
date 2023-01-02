# Paths
RESOURCES = resources

ISO_DIR = isodir
ISO_NAME = axle.iso

OBJ_DIR = .objs
SRC_DIR = kernel
COMPILED_RUST_KERNEL_LIBS_DIR = compiled_rust_kernel_libs

ARCH = x86_64

TOOLCHAIN ?= ./$(ARCH)-toolchain

# Compilers and flags
AS = nasm
ASFLAGS = -f elf64
LD = $(TOOLCHAIN)/bin/$(ARCH)-elf-axle-ld

CC = $(TOOLCHAIN)/bin/$(ARCH)-elf-axle-gcc
SYSROOT = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))axle-sysroot/
CFLAGS = -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -g -ffreestanding -std=gnu99 -Wall -Wextra -I$(SRC_DIR) -I$(SYSROOT)/usr/include
LDFLAGS = -ffreestanding -nostdlib -mcmodel=kernel -mno-red-zone -nostdlib -z max-page-size=0x1000 -lgcc -T $(RESOURCES)/linker.ld

# Tools
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue

GDB = $(TOOLCHAIN)/bin/$(ARCH)-elf-gdb
GDB_FLAGS = -x script.gdb

# Functions
findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
find_archives = $(wildcard $(1)/*.a)
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

# Source files
PATHS = $(shell find $(SRC_DIR) -type d -print)

AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))

OBJECTS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES)))
OBJECTS := $(foreach f,$(OBJECTS),$(if $(filter extern,$(subst /, ,$f)),,$f))

COMPILED_RUST_KERNEL_LIBS = $(wildcard $(COMPILED_RUST_KERNEL_LIBS_DIR)/*.a)

# Rules
all: $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p `dirname $@`
	$(AS) $(ASFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	@mkdir -p `dirname $@`
	$(CC) $(CFLAGS) -o $@ -c $<

$(ISO_DIR)/boot/axle.bin: $(OBJECTS) $(COMPILED_RUST_KERNEL_LIBS)
	@mkdir -p `dirname $(ISO_DIR)/boot/axle.bin`
	$(CC) $(LDFLAGS) -o $(ISO_DIR)/boot/axle.bin $^

$(ISO_DIR)/boot/grub/grub.cfg: $(RESOURCES)/grub.cfg
	@mkdir -p `dirname $@`
	cp $^ $@

dbg:
	$(GDB) $(GDB_FLAGS)

clean:
	@rm -rf $(OBJECTS) $(ISO_DIR) $(ISO_NAME)
