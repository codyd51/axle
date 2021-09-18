#ifndef VFS_H
#define VFS_H

#include "initrd.h"
#include "fat.h"

typedef union fs_node {
    fs_base_node_t base;
    initrd_fs_node_t initrd;
    fat_fs_node_t fat;
} fs_node_t;

#endif