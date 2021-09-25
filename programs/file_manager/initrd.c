#include <stdio.h>

#include "file_manager_messages.h"
#include "initrd.h"

fs_base_node_t* initrd_parse_from_amc(fs_base_node_t* vfs_root, amc_initrd_info_t* initrd_info) {
	char* initrd_path = "initrd";
	fs_base_node_t* initrd_root = fs_node_create__directory(vfs_root, initrd_path, strlen(initrd_path));

	initrd_header_t* header = (initrd_header_t*)initrd_info->initrd_start;
	uint32_t offset = initrd_info->initrd_start + sizeof(initrd_header_t);
	for (uint32_t i = 0; i < header->nfiles; i++) {
		initrd_file_header_t* file_header = (initrd_file_header_t*)offset;

		assert(file_header->magic == HEADER_MAGIC, "Initrd file header magic was wrong");

		initrd_fs_node_t* fs_node = (initrd_fs_node_t*)fs_node_create__file(initrd_root, file_header->name, strlen(file_header->name));
		fs_node->base.type= FS_NODE_TYPE_INITRD;
		fs_node->initrd_offset = file_header->offset + initrd_info->initrd_start;
		fs_node->size = file_header->length;

		offset += sizeof(initrd_file_header_t);
	}
	return initrd_root;
}

uint8_t* initrd_read_file(initrd_fs_node_t* fs_node, uint32_t* out_file_size) {
    printf("Reading initrd file %s, initrd offset: 0x%08lx\n", fs_node->base.name, fs_node->initrd_offset);

    *out_file_size = fs_node->size;
    uint8_t* out_buf = calloc(1, fs_node->size);
    memcpy(out_buf, (uint8_t*)fs_node->initrd_offset, fs_node->size);
    return out_buf;
}
