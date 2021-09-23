#ifndef VFS_H
#define VFS_H

#include "initrd.h"
#include "fat.h"

typedef union fs_node {
    fs_base_node_t base;
    initrd_fs_node_t initrd;
    fat_fs_node_t fat;
} fs_node_t;

fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, char* name, uint32_t name_len);
fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, char* name, uint32_t name_len);

void vfs_launch_program_by_node(fs_node_t* node);
fs_node_t* vfs_find_node_by_name(char* name);

fs_base_node_t* vfs_root_node(void);
// Friend function for file_manager.c
void vfs__set_root_node(fs_base_node_t* node);

#endif