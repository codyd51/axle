#include <stdint.h>
#include <stddef.h>

#include <kernel/boot_info.h>
#include <kernel/vmm/vmm.h>
#include <std/memory.h>

#include "vfs.h"
#include "initrd.h"

static fs_node_t* root_fs_node = NULL;

static fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* dir = calloc(1, sizeof(fs_node_t));
	dir->type = FS_NODE_TYPE_BASE;
	dir->is_directory = true;
	dir->children = array_m_create(64);

	dir->parent = parent;
	if (parent) {
		array_m_insert(parent->children, dir);
	}

	strncpy(dir->name, name, name_len);
	return dir;
}

static fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* file = calloc(1, sizeof(fs_node_t));
	file->type = FS_NODE_TYPE_BASE;
	file->is_directory = false;
	file->children = NULL;

	file->parent = parent;
	if (parent) {
		array_m_insert(parent->children, file);
	}

	strncpy(file->name, name, name_len);
	return file;
}

static void _parse_initrd(fs_base_node_t* initrd_root) {
	boot_info_t* boot_info = boot_info_get();
	assert(boot_info->initrd_start && boot_info->initrd_end && boot_info->initrd_size, "Initrd memory-map not found.");

	initrd_header_t* header = (initrd_header_t*)boot_info->initrd_start;
	uint32_t offset = boot_info->initrd_start + sizeof(initrd_header_t);
	for (uint32_t i = 0; i < header->nfiles; i++) {
		initrd_file_header_t* file_header = (initrd_file_header_t*)offset;

		assert(file_header->magic == HEADER_MAGIC, "Initrd file header magic was wrong");

		initrd_fs_node_t* fs_node = (initrd_fs_node_t*)fs_node_create__file(initrd_root, file_header->name, strlen(file_header->name));
		fs_node->type = FS_NODE_TYPE_INITRD;
		fs_node->initrd_offset = file_header->offset + boot_info->initrd_start;
		fs_node->size = file_header->length;

		offset += sizeof(initrd_file_header_t);
	}
}

static void _print_tabs(uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		putchar('\t');
	}
}

static void _print_fs_tree(fs_node_t* node, uint32_t depth) {
	_print_tabs(depth);
	const char* type = node->base.is_directory ? "Dir" : "File";
	printf("<%s %s", type, node->base.name);
	if (node->base.type == FS_NODE_TYPE_INITRD) {
		printf(", Start = 0x%08x, Len = 0x%08x>\n", node->initrd.initrd_offset, node->initrd.size);
	}
	else if (node->base.type == FS_NODE_TYPE_ROOT) {
		printf(" (Root)>\n");
	}
	else if (node->base.type == FS_NODE_TYPE_BASE) {
		printf(">\n");
	}
	else {
		assert(false, "Unknown fs node type");
	}

	if (node->base.children) {
		for (uint32_t i = 0; i < node->base.children->size; i++) {
			fs_node_t* child = array_m_lookup(node->base.children, i);
			_print_fs_tree(child, depth + 1);
		}
	}
}

void vfs_init(void) {
	char* root_path = "/";
	fs_base_node_t* root = fs_node_create__directory(NULL, root_path, strlen(root_path));
	root_fs_node = root;
	root->type = FS_NODE_TYPE_ROOT;

	char* initrd_path = "initrd";
	fs_base_node_t* initrd_root = fs_node_create__directory(root, initrd_path, strlen(initrd_path));
	_parse_initrd(initrd_root);
	_print_fs_tree((fs_node_t*)root, 0);
}

initrd_fs_node_t* vfs_find_initrd_node_by_name(char* name) {
	fs_base_node_t* initrd = array_m_lookup(root_fs_node->base.children, 0);
	for (uint32_t i = 0; i < initrd->children->size; i++) {
		initrd_fs_node_t* child = array_m_lookup(initrd->children, i);
		if (!strcmp(child->name, name)) {
			return child;
		}
	}
	return NULL;
}
