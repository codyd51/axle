# Paths
RESOURCES = resources

ISO_DIR = isodir
ISO_NAME = axle.iso

OBJ_DIR = .objs
SRC_DIR = kernel

ARCH = x86_64

TOOLCHAIN ?= ./$(ARCH)-toolchain

# Compilers and flags
AS = nasm
# AFLAGS = -f elf
AFLAGS = -f elf64
LD = $(TOOLCHAIN)/bin/$(ARCH)-elf-axle-ld

CC = $(TOOLCHAIN)/bin/$(ARCH)-elf-axle-gcc
SYSROOT = $(dir $(realpath $(firstword $(MAKEFILE_LIST))))axle-sysroot/
CFLAGS = -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -g -ffreestanding -std=gnu99 -Wall -Wextra -I$(SRC_DIR) -I$(SYSROOT)/usr/include
LDFLAGS = -ffreestanding -nostdlib -mcmodel=kernel -mno-red-zone -nostdlib -z max-page-size=0x1000 -lgcc -T $(RESOURCES)/linker.ld

# Tools
ISO_MAKER = $(TOOLCHAIN)/bin/grub-mkrescue
# Always use x86_64 even when targeting i686, as doing so allows us to use the host CPU
# i686 code will be run in compatibility mode by the host CPU
EMULATOR = qemu-system-x86_64
FSGENERATOR = fsgen

GDB = $(TOOLCHAIN)/bin/$(ARCH)-elf-gdb
GDB_FLAGS = -x script.gdb

# Functions
findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

# Source files
PATHS = $(shell find $(SRC_DIR) -type d -print)
PATHS := $(foreach f,$(PATHS),$(if $(filter extern,$(subst /, ,$f)),,$f))

AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))

OBJECTS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES)))
OBJECTS := $(foreach f,$(OBJECTS),$(if $(filter extern,$(subst /, ,$f)),,$f))

INITRD = ./initrd

# Compilation flag helpers
ifdef BMP
CFLAGS += -DBMP
endif

EMFLAGS = -D qemu.log -serial file:syslog.log -monitor stdio -d cpu_reset -no-reboot -m 2048m -hda axle-hdd.img
ifdef debug
EMFLAGS += -s -S
endif
EMFLAGS += -netdev vmnet-macos,id=vmnet,mode=bridged -device rtl8139,netdev=vmnet -object filter-dump,netdev=vmnet,id=dump,file=dump.dat -vga vmware -accel hvf -cpu host

# Rules
all: $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg 

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

$(ISO_DIR)/boot/initrd.img: $(FSGENERATOR)
	@./$(FSGENERATOR) $(INITRD); mv $(INITRD).img $@

$(ISO_NAME): $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg 
	$(ISO_MAKER) -d ./$(ARCH)-toolchain/lib/grub/i386-pc -o $@ $(ISO_DIR)

run: $(ISO_NAME)
	echo 'Run starting' > syslog.log
	echo '' > syslog.log
	echo '' > syslog.log
	echo '' > syslog.log
	$(EMULATOR) $(EMFLAGS) -cdrom $^

dbg:
	$(GDB) $(GDB_FLAGS)

clean:
	@rm -rf $(OBJECTS) $(ISO_DIR) $(ISO_NAME) $(FSGENERATOR)

