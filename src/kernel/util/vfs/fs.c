#include "fs.h"
#include <std/std.h>
#include <std/math.h>
#include <kernel/util/multitasking/fd_entry.h>

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void open_fs(fs_node_t* node, uint8_t read, uint8_t write) {
	//does the node have an open callback?
	if (node->open) {
		return node->open(node);
	}
}
#pragma GCC diagnostic pop

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
FILE* fopen(char* filename, char* mode) {
	//skip preceding ./
	//TODO properly traverse file paths
	while (!isalpha(*filename)) {
		filename++;
	}

	fs_node_t* file = finddir_fs(fs_root, filename);
	if (!file) {
		return NULL;
	}
	FILE* stream = (FILE*)kmalloc(sizeof(FILE));
	memset(stream, 0, sizeof(FILE));
	stream->node = file;
	stream->fpos = 0;

	fd_entry file_fd;
	file_fd.type = FILE_TYPE;
	file_fd.payload = stream;
	stream->fd = fd_add(task_with_pid(getpid()), file_fd);

	return stream;
}
#pragma GCC diagnostic pop

void fclose(FILE* stream) {
	kfree(stream);
}

int fseek(FILE* stream, long offset, int origin) {
	if (!stream) return 1;

	switch (origin) {
		case SEEK_SET:
			stream->fpos = offset;
			break;
		case SEEK_CUR:
			stream->fpos += offset;
			break;
		case SEEK_END:
		default:
			stream->fpos = stream->node->length - offset;
			break;
	}
	stream->fpos = MAX(stream->fpos, (uint32_t)0);
	stream->fpos = MIN(stream->fpos, stream->node->length);
	return 0;
}

int ftell(FILE* stream) {
	if (!stream) return 1;
	return stream->fpos;
}

uint8_t fgetc(FILE* stream) {
	uint8_t ch;
	uint32_t sz = read_fs(stream->node, stream->fpos++, 1, &ch);
	if ((int8_t)ch == EOF || !sz) return EOF;
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
	uint32_t i = 0;
	for (; i < count; i++) {
		for (uint32_t j = 0; j < size; j++) {
			int idx = (i * size) + j;
			chbuf[idx] = fgetc(stream);
			if (chbuf[idx] == EOF) {
				break;
			}
		}
	}
	chbuf[i] = '\0';
	return i;
		/*
		unsigned char buf;
		
		int read_bytes = read_fs(stream->node, stream->fpos++, size, (uint8_t*)&buf);
		sum += read_bytes;
		chbuf[i] = buf;
		//if we read less than we asked for,
		//we hit the end of the file
		if (read_bytes < size) {
			break;
		}
		sum += read_fs(stream->node, stream->fpos++, size, (uint8_t*)&buf);
		//TODO check for eof and break early
		chbuf[i] = buf;
		int read_bytes = read_fs(stream->node, stream->fpos++, size, (uint8_t*)chbuf);
		sum += read_bytes;
		//chbuf[i] = buf;
		//if we read less than we asked for,
		//we hit the end of the file
		if (read_bytes < size) {
			break;
		}
	}
	return sum;
		*/
}

