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

int is_file(const char* path) {
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISREG(path_stat.st_mode);
}

void write_dir(const char* name) {
	rd_header headers[HEADERS_MAX];
	//initial file offset is size of initrd header * max headers + actual header count
	unsigned int off = sizeof(rd_header) * HEADERS_MAX + sizeof(int);
	
	DIR* dp;
	struct dirent* ep;
	dp = opendir(name);
	int nheaders = 0;
	if (dp) {
		while ((ep = readdir(dp))) {	
			const char* name = ep->d_name;
			printf("name: %s\n", name);
			if (!is_file(name)) {
				printf("Found directory %s, skipping for now\n", name);
				continue;
			}

			printf("writing file %s at 0x%x\n", name, off);
			strcpy(headers[nheaders].name, name);
			//add null byte to end of filename
			headers[nheaders].name[strlen(ep->d_name)] = 0;

			//write offset into initrd
			headers[nheaders].offset = off;
			FILE* stream = fopen(name, "r");
			if (!stream) {
				printf("Error: file not found: %s\n", name);
				//return 1;
			}
			
			//find length of file 
			fseek(stream, 0, SEEK_END);
			headers[nheaders].length = ftell(stream);
			printf("length is %d\n", headers[nheaders].length);

			off += headers[nheaders].length;
			fclose(stream);
			headers[nheaders].magic = HEADER_MAGIC;

			nheaders++;	
		}
	}
	else {
		perror("Couldn't find directory");
	}
	
	FILE* wstream = fopen("./initrd.img", "w");
	//write number of headers first
	fwrite(&nheaders, sizeof(int), 1, wstream);
	//write header info
	fwrite(headers, sizeof(rd_header), HEADERS_MAX, wstream);

	//write actual file data to initrd
	printf("writing %d headers to initrd\n", nheaders);
	for (int i = 0; i < nheaders; i++) {
		const char* name = headers[i].name;
		FILE* stream = fopen(name, "r");

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
