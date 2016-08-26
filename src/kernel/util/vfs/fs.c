#include "fs.h"
#include <std/std.h>

fs_node_t* fs_root = 0; //filesystem root

uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	//does the node have a read callback?
	if (node->read) {
		return node->read(node, offset, size, buffer);
	}
	return 0;
}

uint32_t write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
	//does the node have a write callback?
	if (node->write) {
		return node->write(node, offset, size, buffer);
	}
	return 0;
}

void open_fs(fs_node_t* node, uint8_t read, uint8_t write) {
	//does the node have an open callback?
	if (node->open) {
		return node->open(node);
	}
}

void close_fs(fs_node_t* node) {
	//does the node have a close callback?
	if (node->close) {
		return node->close(node);
	}
}

struct dirent* readdir_fs(fs_node_t* node, uint32_t index) {
	//is the node a directory, and does it have a callback?
	if ((node->flags & 0x7) == FS_DIRECTORY && node->readdir) {
		return node->readdir(node, index);
	}
	return 0;
}

fs_node_t* finddir_fs(fs_node_t* node, char* name) {
	//is the node a directory, and does it have a callback?
	if ((node->flags & 0x7) == FS_DIRECTORY && node->finddir) {
		return node->finddir(node, name);
	}
	return 0;
}

FILE* fopen(char* filename, char* mode) {
	fs_node_t* file = finddir_fs(fs_root, filename);
	if (!file) {
		printf_err("Couldn't find file %s", filename);
		return NULL;
	}
	FILE* stream = (FILE*)kmalloc(sizeof(FILE));
	memset(stream, 0, sizeof(FILE));
	stream->node = file;
	stream->fpos = 0;
	return stream;
}

uint8_t fgetc(FILE* stream) {
	uint8_t ch;
	uint32_t sz = read_fs(stream->node, stream->fpos++, 1, &ch);
	if (ch == EOF || !sz) return EOF;
	return ch;
}

char* fgets(char* buf, int count, FILE* stream) {
	int c;
	char* cs = buf;
	while (--count > 0 && (c = fgetc(stream)) != EOF) {
		//place input char in current position, then increment
		//quit on newline
		if ((*cs++ = c) == '\n') break;
	}
	*cs = '\0';
	return (c == EOF && cs == buf) ? (char*)EOF : buf;
}

uint32_t fread(void* buffer, uint32_t size, uint32_t count, FILE* stream) {
	unsigned char* chbuf = (unsigned char*)buffer;
	uint32_t sum;
	for (int i = 0; i < count; i++) {
		unsigned char buf;
		sum += read_fs(stream->node, stream->fpos++, size, (uint8_t*)&buf);
		//TODO check for eof and break early
		chbuf[i] = buf;
	}
	return sum;
}
