#ifndef VFS_H
#define VFS_H

#include "initrd.h"

typedef union fs_node {
    fs_base_node_t base;
    initrd_fs_node_t initrd;
} fs_node_t;

void vfs_init(void);
initrd_fs_node_t* vfs_find_initrd_node_by_name(char* name);

#endif