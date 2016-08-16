#ifndef FS_H
#define FS_H

#include <std/common.h>

#define FS_FILE		0x01
#define FS_DIRECTORY	0x02
#define FS_CHARDEVICE	0x03
#define FS_BLOCKDEVICE	0x04
#define FS_PIPE		0x05
#define FS_SYMLINK	0x06
#define FS_MOUNTPOINT	0x08 //use 8 instead of 7 so we can OR with FS_DIRECTORY

struct fs_node;

typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef struct dirent * (*readdir_type_t)(struct fs_node*, uint32_t);
typedef struct fs_node * (*finddir_type_t)(struct fs_node*, char* name);

typedef struct fs_node {
	char name[128]; 	//filename
	uint32_t mask;		//permission mask
	uint32_t uid;		//owning user
	uint32_t gid;		//owning group
	uint32_t flags;		//includes node type
	uint32_t inode;		//allows fs to identify files
	uint32_t length;	//size of file (bytes)
	uint32_t impl;		
	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;
	struct fs_node* ptr;	//used by mountpoints and symlinks
	struct fs_node* parent; //parent directory of this node
} fs_node_t;

typedef struct file_t {
	uint32_t fpos;
	fs_node_t* node;
} FILE;

struct dirent {
	char name[128];		//filename
	uint32_t ino;		//inode number
};

extern fs_node_t* fs_root; //filesystem root

//standard read/write/open/close
//note: these are suffixed with _fs to distinguish from the functions
//that deal with file descriptors, not file nodes
uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
void open_fs(fs_node_t* node, uint8_t read, uint8_t write);
void close_fs(fs_node_t* node);
struct dirent* readdir_fs(fs_node_t* node, uint32_t index);
fs_node_t* finddir_fs(fs_node_t* node, char* name);

FILE* fopen(const char* filename, const char* mode);
int fgetc(FILE* stream);

#endif
