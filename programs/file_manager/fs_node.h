#ifndef FS_NODE_H
#define FS_NODE_H

#include <stdlibadd/array.h>

typedef enum fs_node_type {
    FS_NODE_TYPE_BASE = 0,
    FS_NODE_TYPE_ROOT = 1,
    FS_NODE_TYPE_INITRD = 2
} fs_node_type_t;

typedef struct fs_base_node {
    // Common fields
    fs_node_type_t type;
    char name[64];
    struct fs_base_node* parent;
    bool is_directory;
    array_t* children;
} fs_base_node_t;

#endif