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

EMFLAGS = -D qemu.log -serial file:syslog.log -monitor stdio -d cpu_reset -no-reboot -m 512m
ifdef debug
EMFLAGS += -s -S
endif
#ifdef net
# EMFLAGS += -net nic,model=ne2k_pci
# https://www.qemu.org/2018/05/31/nic-parameter/
# EMFLAGS += -nic user,model=rtl8139
# EMFLAGS += -netdev tap,id=mynet,script=./qemu-ifup.sh,downscript=./qemu-ifdown.sh -net nic,model=rtl8139
# EMFLAGS += -net tap
# EMFLAGS += -netdev tap,id=network0,netdev=network0
# EMFLAGS += -net tap,id=rtl8139.0,script=./tap-up.sh,downscript=./tap-down.sh
# EMFLAGS += -net tap,script=./tap-up.sh,downscript=./tap-down.sh -net nic,model=rtl8139
#EMFLAGS += -nic tap,model=rtl8139,script=./tap-up.sh,downscript=./tap-down.sh, -object filter-dump,id=tap0
#EMFLAGS += -nic tap,model=rtl8139,script=./tap-up.sh,downscript=./tap-down.sh, -object filter-dump,id=tap0

# EMFLAGS += -netdev tap,id=tap0,script=./tap-up.sh,downscript=./tap-down.sh -device rtl8139,netdev=tap0 -object filter-dump,id=f1,netdev=tap0,file=dump.txt
# EMFLAGS += -netdev tap,id=mynet0,script=./tap-up.sh,downscript=./tap-down.sh -device rtl8139,netdev=mynet0 -object filter-dump,id=mydump1,netdev=mynet0,file=dump.dat
# EMFLAGS += -netdev tap,id=tap0,script=./tap-up.sh,downscript=./tap-down.sh -device rtl8139,netdev=tap0 -object filter-dump,id=tap0,netdev=tap0,file=dump.dat
# EMFLAGS += -nic tap,model=rtl8139,script=./tap-up.sh,downscript=./tap-down.sh,mac=54:54:00:55:55:55,id=test,ifname=tap0 -object filter-dump,id=test,netdev=test,file=dump.dat
# EMFLAGS +=  -net tap,ifname=tap0,id=vtap0,script=./tap-up.sh,downscript=./tap-down.sh -object filter-dump,file=dump.dat,id=tap0,netdev=vtap0 -net nic,model=rtl8139,netdev=vtap0
# EMFLAGS += -netdev tap,script=./tap-up.sh,downscript=./tap-down.sh,id=test -device rtl8139,netdev=test -object filter-dump,netdev=test,file=dump.txt,id=f1
# EMFLAGS += -netdev user,id=test -device rtl8139,netdev=test -object filter-dump,netdev=test,file=dump.txt,id=f1
#endif
# EMFLAGS += -netdev tap,script=./qemu-ifup.sh,downscript=./qemu-ifdown.sh,id=test -device rtl8139,netdev=test -object filter-dump,id=f1,netdev=test,file=dump.txt
# EMFLAGS += -netdev user,id=mynet0 -device rtl8139,netdev=mynet0 -object filter-dump,id=f1,netdev=mynet0,file=dump.txt
EMFLAGS += -nic tap,model=rtl8139,script=./qemu-ifup.sh,downscript=./qemu-ifdown.sh,id=u1 -object filter-dump,id=f1,netdev=u1,file=dump.dat


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
	echo 'Run starting' > syslog.log
	echo '' > syslog.log
	echo '' > syslog.log
	echo '' > syslog.log
	sudo $(EMULATOR) $(EMFLAGS) -cdrom $^

dbg:
	$(GDB) $(GDB_FLAGS)

clean:
	@rm -rf $(OBJECTS) $(ISO_DIR) $(ISO_NAME) $(FSGENERATOR)

