#ifndef INITRD_H
#define INITRD_H

#include <stdint.h>
#include <stdbool.h>

#include <kernel/amc.h>

#include "fs_node.h"

#define HEADER_MAGIC 0xBF

typedef struct {
	uint32_t nfiles;	//# of files in ramdisk
} initrd_header_t;

typedef struct {
	uint8_t magic;	//magic number
	char name[64]; //filename
	uint32_t offset; //offset in initrd that file starts
	uint32_t length; //length of file
} initrd_file_header_t;

typedef struct initrd_fs_node {
    fs_base_node_t base;
    // Private fields
    uintptr_t initrd_offset;
    uint32_t size;
} initrd_fs_node_t;

fs_base_node_t* initrd_parse_from_amc(fs_base_node_t* initrd_root, amc_initrd_info_t* initrd_info);
uint8_t* initrd_read_file(initrd_fs_node_t* fs_node, uint32_t* out_file_size);

#endif