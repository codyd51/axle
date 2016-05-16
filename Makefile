AS = nasm
AFLAGS = -f elf

CC = i686-elf-gcc
CFLAGS = -std=gnu99 -ffreestanding -Wall -Wextra -I ./src

findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

OBJ_DIR = .objs
SRC_DIR = src
PATHS = $(shell find $(SRC_DIR) -type d -print)
AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))
OBJECTS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES)))

all: axle 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p `dirname $@`
	$(AS) $(AFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	@mkdir -p `dirname $@`
	$(CC) $(CFLAGS) -o $@ -c $<

axle: $(OBJECTS)
	$(CC) -T src/boot/linker.ld -o axle.bin -ffreestanding -nostdlib -lgcc $^

clean:
	rm $(OBJECTS)
