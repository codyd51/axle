#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <stdlibadd/assert.h>
#include <agx/lib/shapes.h>
#include <awm/awm_messages.h>

#include "util.h"
#include "vfs.h"

static fs_base_node_t* _g_root_fs_node = NULL;

fs_base_node_t* vfs_root_node(void) {
	return _g_root_fs_node;
}

void vfs__set_root_node(fs_base_node_t* node) {
	_g_root_fs_node = node;
}

fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, const char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* dir = calloc(1, sizeof(fs_node_t));
	dir->type = FS_NODE_TYPE_BASE;
	dir->is_directory = true;
	dir->children = array_create(128);

	dir->parent = parent;
	if (parent) {
		array_insert(parent->children, dir);
	}

	strncpy(dir->name, name, name_len);
	return dir;
}

fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, const char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* file = calloc(1, sizeof(fs_node_t));
	file->type = FS_NODE_TYPE_BASE;
	file->is_directory = false;
	file->children = NULL;

	file->parent = parent;
	if (parent) {
		array_insert(parent->children, file);
	}

	strncpy(file->name, name, name_len);
	return file;
}

void vfs_launch_program_by_node(fs_node_t* node) {
	assert(node->base.type == FS_NODE_TYPE_INITRD, "Can only launch initrd programs");
	amc_exec_buffer_cmd_t cmd = {0};
	cmd.event = AMC_FILE_MANAGER_EXEC_BUFFER;
	cmd.program_name = node->initrd.base.name;
	cmd.buffer_addr = (void*)node->initrd.initrd_offset;
	cmd.buffer_size = node->initrd.size;
	amc_message_send(AXLE_CORE_SERVICE_NAME, &cmd, sizeof(cmd));
}

#define VFS_MAX_PATH_LENGTH 128

fs_node_t* vfs_find_node_by_path_components_in_directory(fs_node_t* directory, array_t* path_components, uint32_t depth) {
	for (uint32_t i = 0; i < directory->base.children->size; i++) {
		fs_node_t* node = array_lookup(directory->base.children, i);

		if (!strncmp(array_lookup(path_components, 0), node->base.name, VFS_MAX_PATH_LENGTH)) {
			//print_tabs(depth);
			//printf("Path component matches\n");

			// Is this the end of the path?
			if (path_components->size == 1) {
				return (fs_node_t*)node;
			}

			// Can we search within a sub-directory?
			if (node->base.is_directory) {
				//print_tabs(depth);
				//printf("Recursing to directory %s\n", node->name);

				array_t* recursed_path = array_create(path_components->size - 1);
				for (uint32_t j = 1; j < path_components->size; j++) {
					array_insert(recursed_path, strdup(array_lookup(path_components, j)));
				}

				fs_node_t* ret = vfs_find_node_by_path_components_in_directory(node, recursed_path, depth + 1);

				array_free_each_element_and_destroy(recursed_path);
				return ret;
			}
		}
	}
	printf("[FS] Failed to find node matching path components\n");
	return NULL;
}

fs_node_t* vfs_find_node_by_path(const char* path) {
	//printf("[FS] Find node by path: %s\n", path);
	fs_node_t* root_node = (fs_node_t*)vfs_root_node();

	if (!strncmp(path, root_node->base.name, VFS_MAX_PATH_LENGTH)) {
		printf("[FS] Returning root node\n");
		return root_node;
	}

	array_t* components = str_split(path, '/');
	fs_node_t* ret = vfs_find_node_by_path_components_in_directory(root_node, components, 1);
	array_free_each_element_and_destroy(components);

	if (ret != NULL) {
		//printf("[FS] Found node at %s!\n", path);
	}
	else {
		printf("[FS] vfs_find_node_by_path(%s) failed\n", path);
	}
	return ret;
}

fat_fs_node_t* vfs_find_node_by_path__fat(const char* path) {
	fs_node_t* node = vfs_find_node_by_path(path);
	assert(node->base.type == FS_NODE_TYPE_FAT, "Expected FAT node!");
	return (fat_fs_node_t*)node;
}

initrd_fs_node_t* vfs_find_node_by_path__initrd(const char* path) {
	fs_node_t* node = vfs_find_node_by_path(path);
	assert(node->base.type == FS_NODE_TYPE_INITRD, "Expected initrd node!");
	return (initrd_fs_node_t*)node;
}

bool vfs_create_directory(const char* path) {
	printf("[FS] vfs_create_directory(%s)\n", path);
	
	// Does the path already exist?
	if (vfs_find_node_by_path(path)) {
		printf("[FS] vfs_create_directory(%s) failed because the specified path already exists\n", path);
		return false;
	}

	// Construct the path to the parent directory
	array_t* components = str_split(path, '/');
	// TODO(PT): Derive the mount point of the ATA drive
	const char* fat_root = "hdd";
	if (strncmp(array_lookup(components, 0), fat_root, strlen(fat_root))) {
		printf("[FS] vfs_create_directory(%s) failed because the path is not within the disk hierarchy\n", path);
		return false;
	}

	char* parent_path = calloc(1, VFS_MAX_PATH_LENGTH);
	for (int32_t i = 0; i < components->size - 1; i++) {
		char* component = array_lookup(components, i);
		snprintf(parent_path, VFS_MAX_PATH_LENGTH, "%s/%s", parent_path, component);
	}

	printf("Parent path: %s\n", parent_path);

	// Get a reference to the parent directory
	fat_fs_node_t* parent_dir = vfs_find_node_by_path__fat(parent_path);
	if (!parent_dir) {
		printf("[FS] vfs_create_directory(%s) failed because the parent directory %s doesn't exist\n", path, parent_path);
		return false;
	}

	// Create the new directory within the parent directory
	fat_fs_node_t* new_directory = fat_create_directory(parent_dir, array_lookup(components, components->size - 1));
	printf("[FS] vfs_create_directory(%s) success! New directory \"%s\" starts at FAT entry #%ld\n", path, new_directory->base.name, new_directory->first_fat_entry_idx_in_file);

	// Free resources
	array_free_each_element_and_destroy(components);
	free(parent_path);
	return true;
}

char* vfs_path_for_node(fs_node_t* node) {
	// Special handling for the filesystem root
	if (node == (fs_node_t*)vfs_root_node()) {
		return strdup("/");
	}

	array_t* components = array_create(64);

	fs_base_node_t* tmp = &node->base;
	while (tmp->parent) {
		array_insert(components, strdup(tmp->name));
		tmp = tmp->parent;
	}

	char* constructed_path = calloc(1, VFS_MAX_PATH_LENGTH);

	for (int32_t i = components->size - 1; i >= 0; i--) {
		char* component = array_lookup(components, i);
		snprintf(constructed_path, VFS_MAX_PATH_LENGTH, "%s/%s", constructed_path, component);
		free(component);
	}
	array_destroy(components);
	return constructed_path;
}

// Unwrap a node, verifying its type
fat_fs_node_t* vfs_fat_node(fs_node_t* node) {
	assert(node->base.type == FS_NODE_TYPE_FAT, "Expected a FAT node");
	return &node->fat;
}

initrd_fs_node_t* vfs_initrd_node(fs_node_t* node) {
	assert(node->base.type == FS_NODE_TYPE_INITRD, "Expected an initrd node");
	return &node->initrd;
}
