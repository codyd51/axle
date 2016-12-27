#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define HEADERS_MAX 64
#define HEADER_MAGIC 0xBF

typedef struct initrd_header {
	unsigned char magic;	//magic number
	char name[64];
	unsigned int offset;	//offset in initrd that the file starts
	unsigned int length;	//length of file
} rd_header;

FILE* openfile(const char* dirname, struct dirent* dir, const char* mode) {
	char pathname[1024]; //should be big enough
	FILE *fp;

	sprintf(pathname, "%s/%s", dirname, dir->d_name);
	printf("trying to open file %s\n", pathname);
	fp = fopen(pathname, mode);
	return fp;
}

void write_dir(const char* dirname) {
	rd_header headers[HEADERS_MAX];
	//initial file offset is size of initrd header * max headers + actual header count
	unsigned int off = sizeof(rd_header) * HEADERS_MAX + sizeof(int);
	
	DIR* dp = opendir(dirname);
	if (!dp) {
		perror("Couldn't find directory");
		return;
	}

	int nheaders = 0;
	struct dirent* ep;
	while ((ep = readdir(dp))) {	
		if (ep->d_type != DT_REG) {
			printf("Found non-file (directory?) %s, skipping for now\n", ep->d_name);
			continue;
		}
		
		char pathname[1024];
		sprintf(pathname, "%s", ep->d_name);
		printf("writing file %s at 0x%x\n", pathname, off);

		strcpy(headers[nheaders].name, pathname);
		//add null byte to end of filename
		headers[nheaders].name[strlen(pathname)] = 0;

		//write offset into initrd
		headers[nheaders].offset = off;

		//open file so we can write binary data to initrd
		//FILE* stream = openfile(dirname, ep, "rb");
		FILE* stream = fopen(pathname, "rb");
		if (!stream) {
			printf("Error: file not found: %s\n", pathname);
			exit(1);
		}
	
		//find length of file 
		fseek(stream, 0, SEEK_END);
		headers[nheaders].length = ftell(stream);
		printf("length is %d\n", headers[nheaders].length);
		
		fclose(stream);

		off += headers[nheaders].length;
		headers[nheaders].magic = HEADER_MAGIC;

		//prepare to write next file
		nheaders++;	
	}
	
	FILE* wstream = fopen("./initrd.img", "w");
	//write number of headers first
	fwrite(&nheaders, sizeof(int), 1, wstream);
	//write header info
	fwrite(headers, sizeof(rd_header), HEADERS_MAX, wstream);

	//write actual file data to initrd
	printf("writing %d headers to initrd\n", nheaders);
	for (int i = 0; i < nheaders; i++) {
		FILE* stream = fopen(headers[i].name, "r");
		if (!stream) {
			printf("Couldn't find file %s!\n", headers[i].name);
			continue;
		}

		unsigned char* buf = (unsigned char*)malloc(headers[i].length);
		fread(buf, 1, headers[i].length, stream);
		fwrite(buf, 1, headers[i].length, wstream);

		fclose(stream);
		free(buf);
	}

	fclose(wstream);
}

int main(int argc, char *argv[]) {
	for (int arg = 1; arg < argc; arg++) {
		write_dir(argv[arg]);
	}
	return EXIT_SUCCESS;
}
