#ifndef INITRD_H
#define INITRD_H

#include <std/common.h>
#include "fs.h"

#define HEADER_MAGIC 0xBF

typedef struct {
	uint32_t nfiles;	//# of files in ramdisk
} initrd_header_t;

typedef struct {
	uint8_t magic;	//magic number
	int8_t name[64]; //filename
	uint32_t offset; //offset in initrd that file starts
	uint32_t length; //length of file
} initrd_file_header_t;

//initializes initial ramdisk
//gets passed address range of multiboot module,
//sets up filesystem root,
//and remaps initrd module to initrd_vmem
void initrd_install(int initrd_loc, int initrd_end, int initrd_vmem);

#endif
