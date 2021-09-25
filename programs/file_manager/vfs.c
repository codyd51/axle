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

fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* dir = calloc(1, sizeof(fs_node_t));
	dir->type = FS_NODE_TYPE_BASE;
	dir->is_directory = true;
	dir->children = array_create(64);

	dir->parent = parent;
	if (parent) {
		array_insert(parent->children, dir);
	}

	strncpy(dir->name, name, name_len);
	return dir;
}

fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, char* name, uint32_t name_len) {
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
	amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &cmd, sizeof(amc_exec_buffer_cmd_t));
}

fs_node_t* vfs_find_node_by_name(char* name) {
	printf("Look for name %s\n", name);
	// TODO(PT): Change to recursive approach to search all directories
    fs_node_t* root_node = vfs_root_node();
	for (uint32_t i = 0; i < root_node->base.children->size; i++) {
		uint32_t max_path_len = 128;
		char* path = calloc(1, max_path_len);
		fs_base_node_t* node = array_lookup(root_node->base.children, i);
		printf("got child node %s\n", node->name);
		snprintf(path, max_path_len, "/%s", node->name);

		printf("Check if root %s matches %s\n", path, name);
		if (!strncmp(path, name, max_path_len)) {
			free(path);
            return (fs_node_t*)node;
		}
		
		if (node->is_directory) {
			for (uint32_t j = 0; j < node->children->size; j++) {
				fs_base_node_t* child = array_lookup(node->children, j);
				char* appended_path = calloc(1, max_path_len);
				snprintf(appended_path, max_path_len, "%s/%s", path, child->name);
				//printf("Check if child %s matches %s\n", appended_path, name);
				if (!strncmp(appended_path, name, max_path_len)) {
					free(path);
					free(appended_path);
					printf("returning node\n");
					return (fs_node_t*)child;
				}

				free(appended_path);
			}
		}

		free(path);
	}
	return NULL;
}
