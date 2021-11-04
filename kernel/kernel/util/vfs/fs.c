#include "fs.h"
#include <std/std.h>
#include <std/math.h>

fs_node_t* fs_root = 0; //filesystem root

uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	Deprecated();
	return 0;
}

uint32_t write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	Deprecated();
	return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void open_fs(fs_node_t* node, uint8_t read, uint8_t write) {
	Deprecated();
}
#pragma GCC diagnostic pop

void close_fs(fs_node_t* node) {
	Deprecated();
}

struct dirent* readdir_fs(fs_node_t* node, uint32_t index) {
	Deprecated();
	return NULL;
}

fs_node_t* finddir_fs(fs_node_t* node, char* name) {
	Deprecated();
	return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
FILE* initrd_fopen(const char* filename, char* mode) {
	Deprecated();
	return NULL;
}

FILE* fopen(const char* filename, char* mode) {
	Deprecated();
	return NULL;
}
#pragma GCC diagnostic pop

int open(const char* filename, int UNUSED(oflag)) {
	Deprecated();
	return 0;
}

void fclose(FILE* stream) {
	Deprecated();
}

int fseek(FILE* stream, long offset, int origin) {
	Deprecated();
	return 0;
}

int ftell(FILE* stream) {
	Deprecated();
	return 0;
}

uint8_t initrd_fgetc(FILE* stream) {
	Deprecated();
	return 0;
}

uint8_t fgetc(FILE* stream) {
	Deprecated();
	return 0;
}

char* fgets(char* buf, int count, FILE* stream) {
	Deprecated();
	return NULL;
}

size_t fwrite(void* ptr, size_t size, size_t count, FILE* stream) {
	Deprecated();
	return 0;
}

uint32_t initrd_fread(void* buffer, uint32_t size, uint32_t count, FILE* stream) {
	Deprecated();
	return 0;
}

uint32_t fread(void* buffer, uint32_t size, uint32_t count, FILE* stream) {
	Deprecated();
	return 0;
}

int getdents(unsigned int fd, struct dirent* dirp, unsigned int count) {
	Deprecated();
	return 0;
}
