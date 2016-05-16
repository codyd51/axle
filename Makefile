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
OBJECTS = $(filter-out boot.s ide.c, $(patsubst src, $(OBJ_DIR), $(call getobjs, $(AXLE_FILES))))

all: axle

%/boot.o: %/boot.s
	$(AS) -o $@ $<

%.o: %.s
	$(AS2) $(AFLAGS) -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

axle: $(OBJECTS) src/boot/boot.o
	$(CC) -T src/boot/linker.ld -o axle.bin -ffreestanding -nostdlib -lgcc $^

clean:
	rm $(OBJECTS)
