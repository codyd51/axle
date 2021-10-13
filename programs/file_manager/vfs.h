#ifndef VFS_H
#define VFS_H

#include "initrd.h"
#include "fat.h"

typedef union fs_node {
    fs_base_node_t base;
    initrd_fs_node_t initrd;
    fat_fs_node_t fat;
} fs_node_t;

typedef enum vfs_node_type {
	VFS_NODE_TYPE_FILE = 0,
	VFS_NODE_TYPE_DIRECTORY = 1,
} vfs_node_type_t;

fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, const char* name, uint32_t name_len);
fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, const char* name, uint32_t name_len);

void vfs_launch_program_by_node(fs_node_t* node);

fs_node_t* vfs_find_node_by_path(const char* path);
// Same as vfs_find_node_by_path, but declare and verify the expected node type
initrd_fs_node_t* vfs_find_node_by_path__initrd(const char* path);
fat_fs_node_t* vfs_find_node_by_path__fat(const char* path);

bool vfs_create_node(const char* path, vfs_node_type_t type);

fs_base_node_t* vfs_root_node(void);
// Friend function for file_manager.c
void vfs__set_root_node(fs_base_node_t* node);

char* vfs_path_for_node(fs_node_t* node);

// Unwrap a node, verifying its type
fat_fs_node_t* vfs_fat_node(fs_node_t* node);
initrd_fs_node_t* vfs_initrd_node(fs_node_t* node);

#endif