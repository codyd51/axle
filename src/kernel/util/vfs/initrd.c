#include "initrd.h"
#include <std/std.h>
#include <kernel/util/paging/paging.h>
#include <kernel/pmm/pmm.h>

initrd_header_t* initrd_header;		//header
initrd_file_header_t* file_headers;	//list of file headers
fs_node_t* initrd_root;			//root directory node
fs_node_t* initrd_dev;			//add directory node for /dev so we can mount devfs later on
fs_node_t* root_nodes;			//list of file nodes
uint8_t nroot_nodes;			//number of file nodes

struct dirent dirent;

static uint32_t initrd_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	initrd_file_header_t header = file_headers[node->inode];
	if (offset >= header.length) {
		*buffer = EOF;
		return 0;
	}
	if (offset + size >= header.length) {
		size = header.length - offset - 1;
	}
	memcpy(buffer, (uint8_t*)(header.offset + offset), size);
	return size;
}

static struct dirent* initrd_readdir(fs_node_t* node, uint32_t index) {
	if (node == initrd_root && index == 0) {
		strcpy(dirent.d_name, "dev");
		dirent.d_name[3] = 0; //null terminate string
		dirent.d_ino = 0;
		return &dirent;
	}

	if (index - 1 >= nroot_nodes) {
		return 0;
	}
	char* name = root_nodes[index - 1].name;
	strcpy(dirent.d_name, name);
	dirent.d_name[strlen(name)] = 0; //null terminate string
	dirent.d_ino = root_nodes[index-1].inode;
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

fs_node_t* initrd_init(uint32_t location) {
	//cast to header at this memory loc
	initrd_header = (initrd_header_t*)location;
	//cast location of file headers
	file_headers = (initrd_file_header_t*)(location + sizeof(initrd_header_t));

	//verify header magic (make sure initrd isn't corrupted
	ASSERT(file_headers->magic == HEADER_MAGIC, "bad initrd magic (%x)", file_headers->magic);

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
	printf("initrd() has %d files\n", initrd_header->nfiles);
	for (uint8_t i = 0 ; i < initrd_header->nfiles; i++) {
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

void initrd_remap(char* initrd_loc, char* initrd_end, char* initrd_vmem) {
	//remap initrd to given vmem address
	uint32_t initrd_size = initrd_end - initrd_loc;

	printf_info("map initrd from: [%x -> %x]\n             to: [%x -> %x]\n", initrd_loc, initrd_end, initrd_vmem, initrd_vmem + initrd_size);
	char* i = initrd_vmem;
	char* src = initrd_loc;
	for (; i < initrd_vmem + initrd_size + PAGE_SIZE; i += PAGE_SIZE, src += PAGE_SIZE) {
		page_t* page = get_page((uint32_t)i, 1, page_dir_current());
		ASSERT(page, "initrd_remap couldn't get page");
		alloc_frame(page, true, false);
		invlpg(i);
		memcpy(i, src, PAGE_SIZE);
	}

	float mb = initrd_size / 1024.0 / 1024.0;
	uint32_t page_count = initrd_size / PAGE_SIZE;
	printf_info("Ramdisk is %f MB (%d pages)", mb, page_count);
}

void initrd_install(uint32_t initrd_loc, uint32_t initrd_end, uint32_t initrd_vmem) {
	//remap initrd in vmem
	initrd_remap((char*)initrd_loc, (char*)initrd_end, (char*)initrd_vmem);
	//and set up filesystem root
	fs_root = initrd_init(initrd_vmem);
}
