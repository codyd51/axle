AS = i686-elf-as
AS2 = nasm
AFLAGS = -f elf

CC = i686-elf-gcc
CFLAGS = -std=gnu99 -ffreestanding -Wall -Wextra -I ./src

findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

PATHS = $(shell find ./src -type d -print)
AXLE_FILES = $(foreach path, $(PATHS), $(call findfiles, $(path)))
OBJ_DIR = .objs
SRC_DIR = src
OBJECTS = $(filter-out boot.s, $(patsubst ./src/%, $(OBJ_DIR)/%, $(call getobjs, $(AXLE_FILES))))

all: axle 

$(OBJ_DIR)/boot/boot.o: $(SRC_DIR)/boot/boot.s
	$(AS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p `dirname $@`
	$(AS2) $(AFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	@mkdir -p `dirname $@`
	$(CC) $(CFLAGS) -o $@ -c $<

axle: $(OBJECTS)
	$(CC) -T src/boot/linker.ld -o axle.bin -ffreestanding -nostdlib -lgcc $^

clean:
	rm $(OBJECTS)
