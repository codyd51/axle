#include "initrd.h"
#include <std/std.h>

initrd_header_t* initrd_header;		//header
initrd_file_header_t* file_headers;	//list of file headers
fs_node_t* initrd_root;			//root directory node
fs_node_t* initrd_dev;			//add directory node for /dev so we can mount devfs later on
fs_node_t* root_nodes;			//list of file nodes
int nroot_nodes;			//number of file nodes

struct dirent dirent;

static uint32_t initrd_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	initrd_file_header_t header = file_headers[node->inode];
	if (offset >= header.length) {
		*buffer = EOF;
		return 0;
	}
	if (offset + size > header.length) {
		size = header.length - offset;
	}
	memcpy(buffer, (uint8_t*)(header.offset + offset), size);
	return size;
}

static struct dirent* initrd_readdir(fs_node_t* node, uint32_t index) {
	if (node == initrd_root && index == 0) {
		strcpy(dirent.name, "dev");
		dirent.name[3] = 0; //null terminate string
		dirent.ino = 0;
		return &dirent;
	}

	if (index - 1 >= nroot_nodes) {
		return 0;
	}
	strcpy(dirent.name, root_nodes[index - 1].name);
	dirent.name[strlen(root_nodes[index-1].name)] = 0; //null terminate string
	dirent.ino = root_nodes[index-1].inode;
	return &dirent;
}

static fs_node_t* initrd_finddir(fs_node_t* node, char* name) {
	if (node == initrd_root && !strcmp(name, "dev")) {
		return initrd_dev;
	}

	for (int i = 0; i < nroot_nodes; i++) {
		if (!strcmp(name, root_nodes[i].name)) {
			return &root_nodes[i];
		}
	}
	return 0;
}

fs_node_t* initrd_install(uint32_t location) {
	//initialize main and file header pointers and populate root directory
	initrd_header = (initrd_header_t*)location;
	file_headers = (initrd_file_header_t*)(location + sizeof(initrd_header_t));

	//initialize root directory
	initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
	strcpy(initrd_root->name, "initrd");
	initrd_root->mask = initrd_root->uid = initrd_root->gid = initrd_root->inode = initrd_root->length = 0;
	initrd_root->flags = FS_DIRECTORY;
	initrd_root->read = 0;
	initrd_root->write = 0;
	initrd_root->open = 0;
	initrd_root->close = 0;
	initrd_root->readdir = &initrd_readdir;
	initrd_root->finddir = &initrd_finddir;
	initrd_root->ptr = 0;
	initrd_root->impl = 0;

	//initializes /dev directory
	initrd_dev = (fs_node_t*)kmalloc(sizeof(fs_node_t));
	strcpy(initrd_dev->name, "dev");
	initrd_dev->mask = initrd_dev->uid = initrd_dev->gid = initrd_dev->inode = initrd_dev->length = 0;
	initrd_dev->flags = FS_DIRECTORY;
	initrd_dev->read = 0;
	initrd_dev->write = 0;
	initrd_dev->open = 0;
	initrd_dev->close = 0;
	initrd_dev->readdir = &initrd_readdir;
	initrd_dev->finddir = &initrd_finddir;
	initrd_dev->ptr = 0;
	initrd_dev->impl = 0;
	initrd_dev->parent = initrd_root;

	root_nodes = (fs_node_t*)kmalloc(sizeof(fs_node_t) * initrd_header->nfiles);
	nroot_nodes = initrd_header->nfiles;

	//for every file
	for (int i = 0 ; i < initrd_header->nfiles; i++) {
		//edit every file's header
		//currently, holds file offset relative to start of ramdisk
		//we want it relative to the start of memory
		file_headers[i].offset += location;
		//create new file node
		strcpy(root_nodes[i].name, (const char*)&file_headers[i].name);
		root_nodes[i].mask = root_nodes[i].uid = root_nodes[i].gid = 0;
		root_nodes[i].length = file_headers[i].length;
		root_nodes[i].inode = i;
		root_nodes[i].flags = FS_FILE;
		root_nodes[i].read = &initrd_read;
		root_nodes[i].write = 0;
		root_nodes[i].open = 0;
		root_nodes[i].close = 0;
		root_nodes[i].readdir = 0;
		root_nodes[i].finddir = 0;
		root_nodes[i].impl = 0;
		root_nodes[i].parent = initrd_root;
	}

	return initrd_root;
}
