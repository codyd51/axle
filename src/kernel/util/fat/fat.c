#include "fat.h"
#include <std/std.h>
#include <stdint.h>
#include <std/memory.h>
#include <std/math.h>
#include <std/string.h>

#define MBR_SECTOR 0
#define SUPERBLOCK_SECTOR 1
#define FAT_SECTOR 2
#define SECTOR_SIZE 512

#define EOF_BLOCK -2
#define FREE_BLOCK -1

#define ROOT_DIRECTORY_SECTOR 0

int sectors_from_bytes(int bytes) {
	int sectors = bytes / SECTOR_SIZE;
	//spill into one more sector?
	if (bytes % SECTOR_SIZE) {
		sectors++;
	}
	return sectors;
}

//current in use file allocation table
//this table uses one uint32_t for each sector
static uint32_t* fat = NULL;
static unsigned char fat_disk;
static void fat_create(int fat_sector_count, unsigned char disk) { 
	fat = kmalloc(fat_sector_count * sizeof(uint32_t));
	memset(fat, FREE_BLOCK, fat_sector_count * sizeof(uint32_t));
	fat_disk = disk;
}

uint32_t* fat_get() {
	return fat;
}

bool is_valid_sector(int sector) {
	return (sector >= 0 && sector < fat_read_sector_count());
}

int fat_read_sector_size() {
	int ssize = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, 0);
	return ssize;
}

int fat_read_sector_count() {
	int fat_size = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, sizeof(uint32_t));
	return fat_size;
}

int fat_read_data_region() {
	int data_region_start = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, sizeof(uint32_t) * 2);
	return data_region_start;
}

void fat_record_superblock(int sector_size, int fat_sector_count) {
	char buf[512];
	memset(buf, 0, sizeof(buf));
	itoa(sector_size, buf);
	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, sector_size, 0);
	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, fat_sector_count, sizeof(uint32_t));

	int data_region_start = fat_sector_count / SECTOR_SIZE;
	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, data_region_start, sizeof(uint32_t) * 2);
}

int fat_file_last_sector(int sector, int* sectors_in_file) {
	uint32_t* fat = fat_get();
	//we always start with at least a sector!
	(*sectors_in_file)++;
	while (fat[sector] != EOF_BLOCK) {
		(*sectors_in_file)++;
		if (fat[sector] == FREE_BLOCK) {
			//this is not an in use sector!
			printf("fat_file_last_sector() error: sector %d is not in use (%d)\n", sector, fat[sector]);
			return -1;
		}
		//follow link
		sector = fat[sector];
	}
	return sector;
}

int fat_first_free_sector() {
	uint32_t* fat = fat_get();
	uint32_t sector_count = fat_read_sector_count() * sizeof(uint32_t);
	for (int i = 0; i < sector_count; i++) {
		if (fat[i] == FREE_BLOCK) {
			//found unused sector!
			return i;
		}
	}
	//no free sectors!
	printf("FAT ran out of usable sectors, %d allocated\n", sector_count);
	return -1;
}

bool fat_free_sector(int sector) {
	int next = fat[sector];
	if (next != EOF_BLOCK && next != FREE_BLOCK) {
		//do we have to walk the entire FAT to find the parent of this node?
		//is there a way to do this faster than O(n)?
		int sector_count = 0;
		fat_file_last_sector(sector, &sector_count);
		for (int i = 0; i < sector_count; i++) {
			if (fat[i] == sector) {
				//found the parent of the sector we're freeing!
				//set the parent's link to the next block of the freed block
				fat[i] = next;
			}
		}
	}
	fat[sector] = FREE_BLOCK;
}

int fat_alloc_sector(int parent) {
	uint32_t sector = fat_first_free_sector();
	uint32_t* fat = fat_get();
	if (is_valid_sector(parent)) {
		int last_used_sector = fat_file_last_sector(parent, NULL);
		fat[last_used_sector] = sector;
	}

	fat[sector] = EOF_BLOCK;
	//fat_flush();
	return sector;
}

void fat_expand_file(uint32_t file, uint32_t size_increase) {
	int sector_count = sectors_from_bytes(size_increase);
	int sectors_in_file = 0;
	int last = fat_file_last_sector(file, &sectors_in_file);
	for (int i = 0; i < sector_count; i++) {
		last = fat_alloc_sector(last);
	}
	//printf("Expanded file %d by %d bytes, chain is now:\n", file, sector_count);
	fat_print_file_links(file);
}

int fat_file_sector_at_index(uint32_t file, uint32_t index) {
	int sector_count = 0;
	fat_file_last_sector(file, &sector_count);
	while (index) {
		if (!is_valid_sector(file)) {
			//didn't reach 'index' before running out of sectors
			return -1;
		}
		index--;
		file = fat[file];
	}
	return file;
}

void fat_shrink_file(uint32_t file, uint32_t size_decrease) {
	int sector_count = sectors_from_bytes(size_decrease);
	int last = fat_file_last_sector(file, NULL);
	for (int i = 0; i < sector_count; i++) {
		fat_free_sector(last - i);
	}
	printf("Shrunk file %d by %d bytes, chain is now:\n", file, sector_count);
	fat_print_file_links(file);
}

void fat_flush() {
	//find sector count from superblock
	//write FAT starting at dedicated sector
	ide_ata_write(fat_disk, FAT_SECTOR, fat_get(), fat_read_sector_count() * sizeof(uint32_t), 0);
}

typedef struct {
	fat_dirent entries[11];
	char reserved[28];
} fat_directory;

void fat_dir_add_file(int dir_start_sector, fat_dirent* new_entry) {
	fat_dirent* free_entry = NULL;
	for (int i = 0; i >= 0; i++) {
		char buf[SECTOR_SIZE];
		int sector = fat_file_sector_at_index(dir_start_sector, i);
		if (sector < 0) {
			//ran out of sectors before we found free space to add a file!
			printf("Directory @ sector %d ran out of space! Expand me\n", dir_start_sector);
			return;
		}
		//look through this sector and see if we have any free space
		fat_directory sector_contents;
		fat_read_file(sector, &sector_contents, sizeof(sector_contents), 0);
		
		for (int j = 0; j < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); j++) {
			fat_dirent entry = sector_contents.entries[j];
			//printf("entry[%d] name %s\n", j, entry.name);
			if (!strlen(entry.name)) {
				//found free entry!
				//printf("fat_dir_add_file() found free entry @ sector %d pos %d\n", sector, j);
				free_entry = &(sector_contents.entries[j]);
				memcpy(free_entry, new_entry, sizeof(fat_dirent));
				fat_write_file(sector, &sector_contents, sizeof(sector_contents), 0);
				return;
			}
		}
	}
	printf("fat_dir_add_file() couldn't find empty entry\n");
	return;
}

void fat_dir_print(int dir_sector) {
	if (!is_valid_sector(dir_sector)) {
		return;
	}
	printf("Directory listing at sector %d:\n", dir_sector);
	fat_directory sector_contents;
	fat_read_file(dir_sector, &sector_contents, sizeof(sector_contents), 0);
	
	for (int i = 0; i < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); i++) {
		fat_dirent entry = sector_contents.entries[i];
		if (strlen(entry.name)) {
			printf("\t");
			if (entry.is_directory) {
				printf("Dir : ");
			}
			else {
				printf("File: ");
			}
			printf("%s (%d bytes) at sector %d\n", entry.name, entry.size, entry.first_sector);
			if (entry.is_directory) {
				fat_dir_print(entry.first_sector);
			}
		}
	}
}

static int sector_for_fat_index(int index) {
	return index + FAT_SECTOR + fat_read_data_region();
}

int fat_write_file(int file_sector, char* buffer, int byte_count, int offset) {
	uint32_t* fat = fat_get();
	int wrote_count = 0;
	int sector_count = sectors_from_bytes(byte_count);
	for (int i = 0; i < sector_count; i++) {
		if (!is_valid_sector(file_sector)) {
			printf("fat_write_file() invalid sector %d\n", file_sector);
			return wrote_count;
		}
		if (offset > 0 && offset < SECTOR_SIZE) {
			offset -= SECTOR_SIZE;
		}
		else {
			char* bufptr = &(buffer[SECTOR_SIZE * i]);
			int bytes_to_write = MIN(SECTOR_SIZE, byte_count);
			int real_sector = sector_for_fat_index(file_sector);
			ide_ata_write(fat_disk, real_sector, bufptr, bytes_to_write, offset);
			wrote_count += bytes_to_write;
			offset = 0;
			byte_count -= bytes_to_write;
		}
		//go to next link in file
		file_sector = fat[file_sector];
	}
	return wrote_count;
}

int fat_read_file(int file_sector, char* buffer, int byte_count, int offset) {
	uint32_t* fat = fat_get();
	int read_count = 0;
	int sector_count = sectors_from_bytes(byte_count);
	for (int i = 0; i < sector_count; i++) {
		if (!is_valid_sector(file_sector)) {
			printf("fat_read_file() invalid sector %d\n", file_sector);
			return read_count;
		}
		if (offset > 0 && offset < SECTOR_SIZE) {
			offset -= SECTOR_SIZE;
		}
		else {
			char* bufptr = &(buffer[SECTOR_SIZE * i]);
			int bytes_to_read = MIN(SECTOR_SIZE, byte_count);
			int real_sector = sector_for_fat_index(file_sector);
			ide_ata_read(fat_disk, real_sector, bufptr, bytes_to_read, offset);
			read_count += bytes_to_read;
			offset = 0;
			byte_count -= bytes_to_read;
		}
		//go to next link in file
		file_sector = fat[file_sector];
	}
	return read_count;
}

int fat_file_create(int file_size) {
	int sector_count = sectors_from_bytes(file_size);
	int last_sector = EOF_BLOCK;
	char buf[SECTOR_SIZE];
	int first_sector = -1;
	memset(buf, 0, sizeof(buf));
	for (int i = 0; i < sector_count; i++) {
		last_sector = fat_alloc_sector(last_sector);
		fat_write_file(last_sector, buf, sizeof(buf), 0);

		if (first_sector < 0) {
			first_sector = last_sector;
		}
	}
	return first_sector;
}

void fat_print_file_links(uint32_t sector) {
	uint32_t* fat = fat_get();
	int sectors_in_file = 0;;
	fat_file_last_sector(sector, &sectors_in_file);
	int filesize = sectors_in_file * SECTOR_SIZE;
	printf("%d sectors (%d bytes): %d->", sectors_in_file, filesize, sector);
	while (fat[sector] != EOF_BLOCK) {
		sector = fat[sector];
		printf("%d->", sector);
	}
	printf("EOF\n");
}

int fat_find_absolute_file(char* name) {
	char* name_copy = strdup(name);
	char** save = NULL;
	char* component = strtok_r(name_copy, "/", save);
	int current_directory = ROOT_DIRECTORY_SECTOR;

	while (component) {
		if (!strcmp(component, ".")) {
			//stay in current directory
			//we don't need to do anything here
		}
		else if (!strcmp(component, "..")) {
			//TODO figure way to go back a directory!
			printf("Traversing up a directory not yet supported.\n");
		}
		else if (strlen(component)) {
			current_directory = fat_dir_read(current_directory, component);
			if (current_directory < 0) {
				//not found!
				return -1;
			}
		}
		component = strtok_r(NULL, "/", save);
	}
	kfree(name_copy);
	return current_directory;
}

int fat_read_absolute_file(char* name, char* buffer, int count, int offset) {
	int sector = fat_find_absolute_file(name);
	int ret = fat_read_file(sector, buffer, count, offset);
	return ret;
}

int fat_write_absolute_file(char* name, char* buffer, int count, int offset) {
	int sector = fat_find_absolute_file(name);
	int ret = fat_write_file(sector, buffer, count, offset);
	return ret;
}

int fat_dir_read(int dir_start_sector, char* name) {
	for (int i = 0; i >= 0; i++) {
		char buf[SECTOR_SIZE];
		int sector = fat_file_sector_at_index(dir_start_sector, i);
		if (sector < 0) {
			//ran out of sectors before we found free space to add a file!
			printf("fat_dir_read(%d) ran out of sectors before finding requested file\n", dir_start_sector);
			return;
		}
		//look through this sector and see if we have any free space
		fat_directory sector_contents;
		fat_read_file(sector, &sector_contents, sizeof(sector_contents), 0);
		
		for (int j = 0; j < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); j++) {
			fat_dirent entry = sector_contents.entries[j];
			//printf("entry[%d] name %s\n", j, entry.name);
			if (!strcmp(entry.name, name)) {
				//found entry we're looking for!
				return entry.first_sector;
			}
		}
	}
	printf("fat_dir_add_file() couldn't find requested file %s\n", name);
	return -1;
}

void fat_format_disk(unsigned char drive) {
	//skip the first 2 blocks, these are reserved for boot sector and super block
	//create FAT in first unused block, block 3
	
	//TODO find disk size!
	//assume 64MB for now
	unsigned int disk_size = 64 * 1024;
	//turn kb into b
	disk_size *= 1024;
	//512b sectors
	unsigned int sectors = disk_size / SECTOR_SIZE;
	printk("sector count for FAT: %d\n", sectors);
	//use drive 0
	fat_create(sectors, 0);
	uint32_t* fat = fat_get();

	char zeroes[SECTOR_SIZE];
	memset(zeroes, 0, sizeof(zeroes));
	ide_ata_write(fat_disk, MBR_SECTOR, zeroes, SECTOR_SIZE, 0);
	ide_ata_write(fat_disk, SUPERBLOCK_SECTOR, zeroes, SECTOR_SIZE, 0);
	fat_record_superblock(SECTOR_SIZE, sectors);

	//after FAT, place root directory entry
	//4kb
	int root_dir = fat_file_create(0x1000);
	fat_dir_print(root_dir);

	int test1 = fat_file_create(0x1000);
	fat_dirent ent;
	strcpy(ent.name, "test1.txt");
	ent.size = SECTOR_SIZE;
	ent.first_sector = test1;
	ent.is_directory = false;
	fat_dir_add_file(root_dir, &ent);

	int test2 = fat_file_create(SECTOR_SIZE);
	strcpy(ent.name, "test2.txt");
	ent.size = SECTOR_SIZE;
	ent.first_sector = test2;
	ent.is_directory = false;
	fat_dir_add_file(root_dir, &ent);

	int dir_test = fat_file_create(SECTOR_SIZE);
	strcpy(ent.name, "usr");
	ent.size = SECTOR_SIZE;
	ent.first_sector = dir_test;
	ent.is_directory = true;
	fat_dir_add_file(root_dir, &ent);

	int bin_dir = fat_file_create(SECTOR_SIZE);
	strcpy(ent.name, "bin");
	ent.size = SECTOR_SIZE;
	ent.first_sector = bin_dir;
	ent.is_directory = true;
	fat_dir_add_file(dir_test, &ent);

	strcpy(ent.name, "test1.txt");
	ent.size = SECTOR_SIZE;
	ent.first_sector = test1;
	ent.is_directory = false;
	fat_dir_add_file(bin_dir, &ent);

	char lorem_buf[4096];
	strcpy(lorem_buf, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur sit amet augue nibh. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla efficitur vel sapien non imperdiet. Ut augue purus, semper eget maximus vitae, accumsan in nisi. Nulla porttitor consequat libero, cursus dignissim tellus. Maecenas vehicula et tortor vitae tristique. Vivamus pretium convallis nisi eget ullamcorper. Phasellus volutpat, mi dictum pretium suscipit, leo lacus convallis urna, eu tincidunt velit risus a lacus. Vestibulum eu ipsum malesuada, bibendum felis ut, blandit mauris. Nunc venenatis lorem convallis vehicula blandit. Morbi id elit eget lacus varius laoreet nec quis tellus. Aliquam pulvinar dolor eu tellus consequat, id accumsan lorem fermentum. Ut pretium molestie risus vitae porta. Donec dapibus augue sed orci viverra, vel ornare justo maximus. In id ligula mi. Morbi sit amet pharetra turpis, sed commodo ipsum. Fusce eleifend fringilla diam ut tristique.");
	//strcpy(lorem_buf, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur sit amet augue nibh. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla efficitur vel sapien non imperdiet. Ut augue purus, semper eget maximus vitae, accumsan in nisi. Nulla porttitor consequat libero, cursus dignissim tellus. Maecenas vehicula et tortor vitae tristique. Vivamus pretium convallis nisi eget ullamcorper. Phasellus volutpat, mi dictum pretium suscipit, leo lacus convallis urna, eu tincidunt velit risus a lacus. Vestibulum eu ipsum malesuada, bibendum felis ut, blandit mauris. Nunc venenatis lorem convallis vehicula blandit. Morbi id elit eget lacus varius laoreet nec quis tellus. Aliquam pulvinar dolor eu tellus consequat, id accumsan lorem fermentum. Ut pretium molestie risus vitae porta. Donec dapibus augue sed orci viverra, vel ornare justo maximus. In id ligula mi. Morbi sit amet pharetra turpis, sed commodo ipsum. Fusce eleifend fringilla diam ut tristique. Nam eu lacus nibh. Quisque volutpat imperdiet libero eu efficitur. Sed non mollis leo. Phasellus sit amet imperdiet nulla, vitae convallis nibh.Vestibulum purus odio, consectetur quis metus in, congue pulvinar odio. Praesent rutrum orci enim, vel pulvinar enim viverra non. Vivamus laoreet tempus quam in suscipit. Integer odio nisi, laoreet gravida nulla eu, sagittis scelerisque augue. Maecenas sollicitudin tempus pulvinar. Praesent magna velit, pulvinar et iaculis ut, facilisis ut enim. Vivamus bibendum purus a risus vehicula, a fringilla turpis porta. Etiam augue tellus, mattis sodales egestas vitae, placerat ut purus. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Praesent bibendum velit elit, eget elementum arcu porta non. Vestibulum congue maximus metus, nec ullamcorper turpis placerat eu.");
	char* path = "/usr/bin/test1.txt";
	fat_write_absolute_file(path, lorem_buf, sizeof(lorem_buf), 0);
	memset(lorem_buf, 0, sizeof(lorem_buf));

	fat_read_absolute_file(path, lorem_buf, sizeof(lorem_buf), 0);
	printf("Read from %s:\n%s\n", path, lorem_buf);
}

