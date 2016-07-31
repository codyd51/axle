#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define HEADERS_MAX 64
#define HEADER_MAGIC 0xBF

typedef struct initrd_header {
	unsigned char magic;	//magic number
	char name[64];
	unsigned int offset;	//offset in initrd that the file starts
	unsigned int length;	//length of file
} rd_header;

int main(int argc, char** argv) {
	int nheaders = argc - 1;
	rd_header headers[HEADERS_MAX];
	unsigned int off = sizeof(rd_header) * HEADERS_MAX + sizeof(int);

	for (int i = 0; i < nheaders; i++) {
		printf("writing file %s at 0x%x\n", argv[i + 1], off);
		strcpy(headers[i].name, argv[i + 1]);
		//add null byte to end of filename
		headers[i].name[strlen(argv[i + 1])] = 0;

		//write offset into initrd
		headers[i].offset = off;
		FILE* stream = fopen(argv[i + 1], "r");
		if (!stream) {
			printf("Error: file not found: %s\n", argv[i + 1]);
			return 1;
		}
		
		//find length of file 
		fseek(stream, 0, SEEK_END);
		headers[i].length = ftell(stream);

		off += headers[i].length;
		fclose(stream);
		headers[i].magic = HEADER_MAGIC;
	}

	FILE* wstream = fopen("./initrd.img", "w");
	
	//write number of headers first
	fwrite(&nheaders, sizeof(int), 1, wstream);

	//write header info
	fwrite(headers, sizeof(rd_header), HEADERS_MAX, wstream);

	//write actual file data to initrd
	for (int i = 0; i < nheaders; i++) {
		FILE* stream = fopen(argv[i + 1], "r");

		unsigned char* buf = (unsigned char*)malloc(headers[i].length);
		fread(buf, 1, headers[i].length, stream);
		fwrite(buf, 1, headers[i].length, wstream);

		fclose(stream);
		free(buf);
	}

	fclose(wstream);

	return 0;
}
