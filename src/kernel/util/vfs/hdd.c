#include "hdd.h"
#include <kernel/drivers/ide/ide.h>
#include <std/std.h>
#include <std/math.h>

//num name[64] start block end block
//example:
//0 index 0 1
//1 test 1 2

void hdd_read(int fileno, char* buf, int count) {
	//each sector is 512 bytes
	//sector count = bytes desired / sector size
	int sector_count = MAX(1, count / 512);
	ide_ata_read(0, fileno, sector_count, buf);
}

void hdd_write(int fileno, char* buf, int count) {
	//each sector is 512 bytes
	//sector count = bytes to write / sector size
	int sector_count = MAX(1, count / 512);
	ide_ata_write(0, fileno, sector_count, buf);
}

int hdd_file_create(const char* name, int size) {
	char buf[2048];
	hdd_read(0, buf, sizeof(buf));
	printk("hdd_file_create() start:\n%s\n", buf);
	//memset(buf, 0, sizeof(buf));
	char* copy = strdup(buf);

	char* save = NULL;
	char* line = strtok_r(copy, "\n", &save);
	char* last_line = NULL;
	while (line) {
		last_line = line;
		line = strtok_r(NULL, "\n", &save);
	}

	//last_line now contains last non-null line
	//parse this line to find where to place next file
	char new_line[128];

	int last_fileno_used = -1;
	if (last_line) {
		last_fileno_used = atoi(last_line);
	}
	int new_fileno = last_fileno_used + 1;
	
	//fileno
	itoa(new_fileno, new_line);
	strcat(new_line, " ");
	//file name
	strcat(new_line, name);
	strcat(new_line, " ");

	//find start sector
	//start sector = end sector of last file
	char* last_word = "0";
	if (last_line) {
		char* word = strtok_r(last_line, " ", &save);
		while (word) {
			last_word = word;
			word = strtok_r(NULL, " ", &save);
		}
	}

	strcat(new_line, last_word);
	strcat(new_line, " ");
	
	//end sector
	//this is start sector + sector size of new file
	//sectors needed = file size / sector size (512 bytes)
	int sectors_needed = size / 512;
	int start_sector = atoi(last_word);
	int end_sector = start_sector + sectors_needed;
	char sect_buf[16];
	itoa(end_sector, sect_buf);
	strcat(new_line, sect_buf);
	strcat(new_line, " ");

	strcat(new_line, "\n");
	strcat(buf, new_line);

	printk("hdd_file_create end:\n%s\n", buf);

	kfree(copy);
	hdd_write(0, buf, sizeof(buf));

	return new_fileno;
}

int hdd_lookup_filename(const char* name) {
	//look through index file to find fileno matching this name
	char index_buf[2048];
	hdd_read(0, index_buf, sizeof(index_buf));
	char* reent;
	char* line = strtok_r(index_buf, "\n", &reent);
	while (line) {
		//filename is second word in index entry
		char* filename_reent;
		char* filename = strtok_r(line, " ", &filename_reent);
		//fileno is first word, save that!
		char* fileno = filename;

		//go to second word
		filename = strtok_r(NULL, " ", &filename_reent);

		if (!strcmp(filename, name)) {
			//found filename we're looking for!
			return atoi(fileno);
		}

		line = strtok_r(NULL, "\n", &reent);
	}
	return -1;
}

int hdd_rename(int fileno, const char* new_name) {
	//TODO implement
	return -1;
}

