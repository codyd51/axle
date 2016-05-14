AS = i686-elf-as
AS2 = nasm
AFLAGS = -f elf

CC = i686-elf-gcc
CFLAGS = -std=gnu99 -ffreestanding -Wall -Wextra

findfiles = $(foreach ext, c s, $(wildcard $(1)/*.$(ext)))
getobjs = $(foreach ext, c s, $(filter %.o,$(patsubst %.$(ext),%.o,$(1))))

AXLE_FILES = $(call findfiles,.)
OBJ_DIR = .objs
OBJECTS = $(filter-out boot.s,$(call getobjs, $(AXLE_FILES)))

all: axle

boot.o: boot.s
	$(AS) -o $@ $<

%.o: %.s
	$(AS2) $(AFLAGS) -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

axle: $(OBJECTS) boot.o
	$(CC) -T linker.ld -o axle.bin -ffreestanding -nostdlib -lgcc $^

clean:
	rm *.o
