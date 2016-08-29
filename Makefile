# Paths
RESOURCES = resources

ISO_DIR = isodir
ISO_NAME = axle.iso

OBJ_DIR = .objs
SRC_DIR = src

# Compilers and flags
AS = nasm
AFLAGS = -f elf

CC = i686-elf-gcc
CFLAGS = -std=gnu99 -ffreestanding -Wall -Wextra -I ./src
LDFLAGS = -ffreestanding -nostdlib -lgcc -T $(RESOURCES)/linker.ld

# Functions
findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

# Source files
PATHS = $(shell find $(SRC_DIR) -type d -print)
AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))
OBJECTS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES)))

# Compilation flag helpers
ifdef BMP
CFLAGS += -DBMP
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

$(ISO_NAME): $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

run: $(ISO_NAME)
	qemu-system-i386 -vga std -cdrom $^

clean:
	rm $(OBJECTS) $(ISO_DIR)/boot/axle.bin $(ISO_DIR)/boot/grub/grub.cfg $(ISO_NAME)
