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
//gets passed address of multiboot module,
//and returns completed filesystem node
fs_node_t* initrd_install(uint32_t location);

#endif
