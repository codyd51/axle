#ifndef INITRD_H
#define INITRD_H

#include <stdint.h>
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
    // Common fields
    fs_node_type_t type;
    char name[64];
    struct fs_base_node* parent;
    bool is_directory;
    array_t* children;
    // Private fields
    uint32_t initrd_offset;
    uint32_t size;
} initrd_fs_node_t;


#endif