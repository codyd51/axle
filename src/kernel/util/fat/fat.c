#include "fat.h"
#include <std/std.h>
#include <stdint.h>
#include <std/memory.h>
#include <std/math.h>
#include <std/string.h>
#include <kernel/drivers/ide/ide.h>

#define MBR_SECTOR 0
#define SUPERBLOCK_SECTOR 1
#define FAT_SECTOR 2
#define SECTOR_SIZE 512

#define EOF_BLOCK (uint32_t)-2
#define FREE_BLOCK (uint32_t)-1

#define ROOT_DIRECTORY_SECTOR 0

int fat_read_file(fat_dirent* file, char* buffer, int byte_count, int offset);
int fat_write_file(fat_dirent* file, char* buffer, int byte_count, int offset);
int fat_dir_read_dirent(fat_dirent* directory, char* name, fat_dirent* store);
bool dirent_for_start_sector(uint32_t desired_sector, fat_dirent* directory, fat_dirent* store);

fat_dirent root_dir;

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

int fat_read_magic() {
	static int magic = 0;
	if (!magic) {
		magic = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, 0);
	}
	return magic;
}

int fat_read_sector_size() {
	static int sector_size = 0;
	if (!sector_size) {
		sector_size = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, sizeof(uint32_t));
	}
	return sector_size;
}

int fat_read_sector_count() {
	static int fat_size = 0;
	if (!fat_size) {
		fat_size = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, sizeof(uint32_t) * 2);
	}
	return fat_size;
}

int fat_read_data_region() {
	int data_region_start = ide_ata_read_int(fat_disk, SUPERBLOCK_SECTOR, sizeof(uint32_t) * 3);
	return data_region_start;
}

#define FAT_MAGIC 0xFEEDFACE
void fat_record_superblock(int sector_size, int fat_sector_count) {
	char buf[SECTOR_SIZE];
	memset(buf, 0, sizeof(buf));
	itoa(sector_size, buf);

	int offset = 0;
	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, FAT_MAGIC, offset);
	offset += sizeof(uint32_t);

	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, sector_size, offset);
	offset += sizeof(uint32_t);

	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, fat_sector_count, offset);
	offset += sizeof(uint32_t);

	int data_region_start = fat_sector_count / SECTOR_SIZE;
	//account for boot sector plus superblock
	//add 1 for boot sector
	//add 1 for superblock
	data_region_start += 2;
	ide_ata_write_int(fat_disk, SUPERBLOCK_SECTOR, data_region_start, offset);
}

int fat_file_last_sector(int sector, uint32_t* sectors_in_file) {
	if (!sectors_in_file) {
		uint32_t local_sectors_in_file;
		sectors_in_file = &local_sectors_in_file;
	}

	*sectors_in_file = 0;
	if (!is_valid_sector(sector)) {
		return EOF_BLOCK;
	}

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
	uint32_t sector_count = fat_read_sector_count();
	for (uint32_t i = 0; i < sector_count; i++) {
		if (fat[i] == FREE_BLOCK) {
			//found unused sector!
			return i;
		}
	}
	//no free sectors!
	printf("FAT ran out of usable sectors, %d allocated\n", sector_count);
	return -1;
}

void fat_dealloc_sector(uint32_t sector) {
	uint32_t next = fat[sector];
	//do we have to walk the entire FAT to find the parent of this node?
	//is there a way to do this faster than O(n)?
	uint32_t sector_count = 0;
	fat_file_last_sector(sector, &sector_count);
	for (uint32_t i = 0; i < sector_count; i++) {
		if (fat[i] == sector) {
			//found the parent of the sector we're freeing!
			//set the parent's link to the next block of the freed block
			fat[i] = next;
		}
	}
	fat[sector] = FREE_BLOCK;
}

int fat_alloc_sector(int parent) {
	uint32_t sector = fat_first_free_sector();
	if (is_valid_sector(parent)) {
		int last_used_sector = fat_file_last_sector(parent, NULL);
		fat[last_used_sector] = sector;
	}

	fat[sector] = EOF_BLOCK;

	//fat_flush();
	return sector;
}

void fat_expand_file(uint32_t file, uint32_t size_increase) {
	uint32_t sector_count = sectors_from_bytes(size_increase);
	uint32_t sectors_in_file = 0;
	int last = fat_file_last_sector(file, &sectors_in_file);
	for (uint32_t i = 0; i < sector_count; i++) {
		last = fat_alloc_sector(last);
	}

	fat_dirent* entry;
	dirent_for_start_sector(file, &root_dir, entry);
	if (!entry) {
		printf("fat_expand_file(%d) couldn't find dirent to expand!\n");
	}
	else {
		entry->size += size_increase;
	}

	fat_print_file_links(file);
}

int fat_file_sector_at_index(uint32_t file, uint32_t index) {
	for (uint32_t i = 0; i < index; i++) {
		if (!is_valid_sector(file)) {
			return EOF_BLOCK;
		}
		file = fat[file];
	}
	return file;
}

void fat_shrink_file(uint32_t file, uint32_t size_decrease) {
	int sector_count = sectors_from_bytes(size_decrease);
	int last = fat_file_last_sector(file, NULL);
	for (int i = 0; i < sector_count; i++) {
		fat_dealloc_sector(last - i);
	}
	printf("Shrunk file %d by %d bytes, chain is now:\n", file, sector_count);
	fat_print_file_links(file);
}

void fat_flush() {
	//find sector count from superblock
	//write FAT starting at dedicated sector
	ide_ata_write(fat_disk, FAT_SECTOR, (uint32_t)fat_get(), fat_read_sector_count() * sizeof(uint32_t), 0);
}

typedef struct {
	fat_dirent entries[11];
	char reserved[28];
} fat_directory;

void fat_dir_add_file(fat_dirent* directory, fat_dirent* new_entry) {
	printk("fat_dir_add_file(%s %d %d)\n", new_entry->name, new_entry->size, new_entry->first_sector);

	fat_dirent* free_entry = NULL;
	for (int i = 0; i >= 0; i++) {
		//look through this sector and see if we have any free space
		fat_directory sector_contents;
		fat_read_file(directory, (char*)&sector_contents, sizeof(sector_contents), i * SECTOR_SIZE);
		
		for (uint32_t j = 0; j < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); j++) {
			fat_dirent entry = sector_contents.entries[j];
			//printf("entry[%d] name %s\n", j, entry.name);
			if (!strlen(entry.name)) {
				//found free entry!
				//printf("fat_dir_add_file() found free entry @ sector %d pos %d\n", sector, j);
				free_entry = &(sector_contents.entries[j]);
				memcpy(free_entry, new_entry, sizeof(fat_dirent));
				fat_write_file(directory, (char*)&sector_contents, sizeof(sector_contents), 0);
				return;
			}
		}
	}
	printk("fat_dir_add_file() couldn't find empty entry\n");
	return;
}

static int sector_for_fat_index(int index) {
	return index + FAT_SECTOR + fat_read_data_region();
}

int fat_write_file(fat_dirent* file, char* buffer, int byte_count, int offset) {
	/*
	if (byte_count + offset > file->size) {
		printf("fat_write_file() needs to handle EOF\n");
		return;
	}
	*/
	int file_sector = file->first_sector;
	int wrote_count = 0;
	int sector_count = sectors_from_bytes(byte_count + offset);
	int sectors_in_offset = sectors_from_bytes(offset);
	for (int i = 0; i < sector_count; i++) {
		if (!is_valid_sector(file_sector)) {
			//were we still skipping to offset?
			if (offset >= SECTOR_SIZE) {
				printk("fat_write_file() offset was larger than file size\n");
				return wrote_count;
			}
			return wrote_count;
		}
		if (offset >= SECTOR_SIZE) {
			offset -= SECTOR_SIZE;
			printk("fat_write_file() skipping sector %d sectors %d\n", file_sector);
		}
		else {
			int offset_within_buf = SECTOR_SIZE * (i - sectors_in_offset);
			char* bufptr = &(buffer[offset_within_buf]);

			int bytes_to_write = MIN(SECTOR_SIZE, byte_count);
			int real_sector = sector_for_fat_index(file_sector);

#ifdef DEBUG
			printk("fat_write_file(%s) sect %d count %d offset %dk @ %x\n", file->name, file_sector, byte_count, offset, bufptr);
			printk("writing %d from %x to IDE sector %d\n", bytes_to_write, bufptr, real_sector);
#endif

			ide_ata_write(fat_disk, real_sector, (uint32_t)bufptr, bytes_to_write, offset);
			wrote_count += bytes_to_write;
			offset = 0;
			byte_count -= bytes_to_write;
		}
		//go to next link in file
		file_sector = fat[file_sector];
	}
	return wrote_count;
}

int fat_read_file(fat_dirent* file, char* buffer, int byte_count, int offset) {
	int file_sector = file->first_sector;
	if (!is_valid_sector(file_sector)) {
		return 0;
	}

	int read_count = 0;
	int sector_count = sectors_from_bytes(byte_count + offset);
	int sectors_in_offset = sectors_from_bytes(offset);
	printk("fat_read_file(%s) read %d sectors start %d\n", file->name, sector_count, file_sector);

	for (int i = 0; i < sector_count; i++) {
		if (!is_valid_sector(file_sector)) {
			printk("fat_read_file() invalid sector %d\n", file_sector);
			return read_count;
		}
		if (offset >= SECTOR_SIZE) {
			offset -= SECTOR_SIZE;
		}
		else {
			int offset_within_buf = SECTOR_SIZE * (i - sectors_in_offset);
			char* bufptr = &(buffer[offset_within_buf]);
			printk("fat_read_file(%s) sect %d count %d offset %d @ %x\n", file->name, file_sector, byte_count, offset, bufptr);
			int bytes_to_read = MIN(SECTOR_SIZE, byte_count);
			int real_sector = sector_for_fat_index(file_sector);
			ide_ata_read(fat_disk, real_sector, (uint32_t)bufptr, bytes_to_read, offset);

			read_count += bytes_to_read;
			byte_count -= bytes_to_read;

			//we've now accounted for within-sector offset, no more need for this
			offset = 0;
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

	fat_dirent entry;
	strcpy((char*)&entry.name, "(creating file)");
	entry.size = file_size;
	entry.first_sector = last_sector;

	for (int i = 0; i < sector_count; i++) {
		last_sector = fat_alloc_sector(last_sector);
		entry.first_sector = last_sector;

		fat_write_file(&entry, buf, sizeof(buf), 0);

		if (first_sector < 0) {
			first_sector = last_sector;
		}
	}
	return first_sector;
}

void fat_print_file_links(uint32_t sector) {
	uint32_t* fat = fat_get();
	uint32_t sectors_in_file = 0;;
	fat_file_last_sector(sector, &sectors_in_file);
	int filesize = sectors_in_file * SECTOR_SIZE;
	printf("%d sectors (%d bytes): %d->", sectors_in_file, filesize, sector);
	while (fat[sector] != EOF_BLOCK) {
		sector = fat[sector];
		printf("%d->", sector);
	}
	printf("EOF\n");
}

int fat_find_absolute_file(char* name, fat_dirent* store) {
	char* name_copy = strdup(name);
	char** save = NULL;
	char* component = strtok_r(name_copy, "/", save);

	fat_dirent current_dir_ent = root_dir;
	int current_directory = current_dir_ent.first_sector;

	if (!store) {
		fat_dirent local_store;
		store = &local_store;
	}

	while (component) {
		if (!strcmp(component, ".")) {
			//stay in current directory
			//we don't need to do anything here
		}
		else if (!strcmp(component, "..")) {
			//TODO figure way to go back a directory!
			printf("Traversing up a directory not yet supported.\n");
			kfree(name_copy);
			return -1;
		}
		else if (strlen(component)) {
			current_directory = fat_dir_read_dirent(&current_dir_ent, component, store);
			printk("traversed to %s %d %d\n", store->name, store->size, store->first_sector);
			current_dir_ent = *store;
			if (current_directory < 0) {
				//not found!
				kfree(name_copy);
				return -1;
			}
		}
		component = strtok_r(NULL, "/", save);
	}
	kfree(name_copy);
	return current_directory;
}

int fat_read_absolute_file(char* name, char* buffer, int count, int offset) {
	fat_dirent entry;
	fat_find_absolute_file(name, &entry);
	printf("fat_read_absolute_file got entry %s %d %d\n", entry.name, entry.size, entry.first_sector);
	int ret = fat_read_file(&entry, buffer, count, offset);
	return ret;
}

int fat_write_absolute_file(char* name, char* buffer, int count, int offset) {
	fat_dirent entry;
	fat_find_absolute_file(name, &entry);
	int ret = fat_write_file(&entry, buffer, count, offset);
	return ret;
}

static void pretty_print_filesize(int size) {
	if (size < 1024) {
		printf("%db", size);
	}
	else if (size < 1024 * 1024) {
		printf("%dkb", size / 1024);
	}
	else {
		printf("%dmb", size / (1024 * 1024));
	}
}

void fat_print_dirent(fat_dirent* entry) {
	if (entry->is_directory) {
		printf("+ ");
	}
	else {
		printf("- ");
	}
	printf("%s (", entry->name);
	pretty_print_filesize(entry->size);
	printf(")\n");
}

int fat_dir_entry_at_index(fat_dirent* directory, int index, fat_dirent* store) {
	for (int i = 0; i >= 0; i++) {
		int sector = fat_file_sector_at_index(directory->first_sector, i);
		if (sector < 0) {
			printf("fat_dir_read(%s) ran out of sectors before finding requested file\n", directory->name);
			return -1;
		}
		//look through this sector and see if we have any free space
		fat_directory sector_contents;
		fat_read_file(directory, (char*)&sector_contents, sizeof(sector_contents), 0);
		
		int indexes_per_sector = sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]);
		int index_within_sector = index % indexes_per_sector;
		fat_dirent entry = sector_contents.entries[index_within_sector];

		if (!strlen(entry.name)) {
			//not an in use entry!
			return -1;
		}

		if (store) {
			memcpy(store, &entry, sizeof(fat_dirent));
		}
		return entry.first_sector;
	}

	printf("fat_dir_add_file() couldn't find requested index %d\n", index);
	return -1;

}

int fat_dir_read_dirent(fat_dirent* directory, char* name, fat_dirent* store) {
	if (!store) {
		fat_dirent local_store;
		store = &local_store;
	}

	printk("fat_dir_read_dirent directory: %s %d %d\n", directory->name, directory->size, directory->first_sector);
	for (int i = 0; i >= 0; i++) {
		if (i * SECTOR_SIZE >= directory->size) {
			break;
		}

		fat_directory sector_contents;
		fat_read_file(directory, (char*)&sector_contents, sizeof(sector_contents), i * SECTOR_SIZE);
		
		for (uint32_t j = 0; j < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); j++) {
			fat_dirent entry = sector_contents.entries[j];
			if (!strcmp(name, entry.name)) {
				//found entry we're looking for!
				strcpy(store->name, &entry.name);
				store->size = entry.size;
				store->first_sector = entry.first_sector;
				printf("fat_dir_read_dirent found entry idx %d %s %d %d \n", j, store->name, store->size, store->first_sector);
				//memcpy(store, &entry, sizeof(fat_dirent));
				return entry.first_sector;
			}
		}
	}
	printf("fat_dir_read_dirent(%s) not found\n", name);
	return -1;
}

void fat_print_directory(fat_dirent* directory, int tablevel, bool print_header) {
	if (print_header) {
		printf("Directory listing of %s\n", directory->name);
		printf("------------------------------------------\n");
	}

	int index = 0;
	while (1) {
		fat_dirent entry;
		int success = fat_dir_entry_at_index(directory, index, &entry);
		if (success < 0) {
			break;
		}

		for (int i = 0; i < tablevel; i++) {
			putchar('\t');
			putchar('\t');
		}
		fat_print_dirent(&entry);
		if (entry.is_directory & 1) {
			fat_print_directory(&entry, tablevel+1, false);
		}

		index++;
	}
}

int fat_dir_read(int dir_sector, char* name) {
	ASSERT(0, "fat_dir_read");
	return -1;
	//return fat_dir_read_dirent(dir_sector, name, NULL);
}

int fat_dir_new_file(fat_dirent* dir, char* name, uint32_t size, bool directory, fat_dirent* store) {
	int new_file = fat_file_create(size);
	if (!store) {
		fat_dirent local_store;
		store = &local_store;
	}
	strncpy(store->name, name, sizeof(store->name));
	store->size = size;
	store->first_sector = new_file;
	store->is_directory = directory;
	fat_dir_add_file(dir, store);

	return new_file;
}

int fat_copy_initrd_file(fat_dirent* dir, char* name, fat_dirent* store) {
	if (!store) {
		fat_dirent local_store;
		store = &local_store;
	}

	FILE* file = initrd_fopen(name, "r");
	if (!file) {
		printf("fat_copy_initrd_file() file %s not found\n", name);
		return -1;
	}
	//find file size
	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fseek(file, 0, SEEK_SET);
	int sector_count = sectors_from_bytes(size);
	printf("\nsize %d (%d sect)\n", size, sector_count);

	int new_file = fat_dir_new_file(dir, name, size, false, store);
	if (!store || !is_valid_sector(new_file)) {
		printf("fat_copy_initrd_file() create new file failed\n");
		return -1;
	}

	char buf[SECTOR_SIZE];
	memset(buf, 0, sizeof(buf));

	for (int i = 0; i < sector_count; i++) {
		int offset = i * SECTOR_SIZE;

		int count = 0;
		for (int j = 0; j < sizeof(buf); j++) {
			if (offset + j >= size) break;

			uint8_t byte = initrd_fgetc(file);
			if (byte == EOF) {
				break;
			}
			buf[count++] = byte;
		}
		fat_write_file(store, buf, count, offset);
	}
	fclose(file);

	return new_file;
}

size_t fat_fread(void* ptr, size_t size, size_t count, FILE* stream) {
	fat_dirent dirent; 
	if (!dirent_for_start_sector(stream->start_sector, &root_dir, &dirent)) {
		printf("fat_fread() dirent_for_start_sector(%d) failed\n", stream->start_sector);
		return 0;
	}

	int read_count = fat_read_file(&dirent, (char*)ptr, count * size, 0);
	stream->fpos += read_count;
	return read_count;
}

size_t fat_fwrite(void* ptr, size_t size, size_t count, FILE* stream) {
	fat_dirent dirent;
	if (!dirent_for_start_sector(stream->start_sector, &root_dir, &dirent)) {
		printf("fat_fwrite() dirent_for_start_sector(%d) failed\n", stream->start_sector);
		return 0;
	}
	int wrote_count = fat_write_file(&dirent, (char*)ptr, count * size, stream->fpos);
	return wrote_count;
}

FILE* fat_fopen(char* filename, char* mode) {
	int fat_sector = fat_find_absolute_file(filename, NULL);
	if (!is_valid_sector(fat_sector)) {
		printf("fat_fopen(%s) No such file or directory\n", filename);
		return NULL;
	}

	FILE* stream = (FILE*)kmalloc(sizeof(FILE));
	memset(stream, 0, sizeof(FILE));
	stream->node = NULL;
	stream->fpos = 0;
	stream->start_sector = fat_sector;

	fd_entry file_fd;
	file_fd.type = FILE_TYPE;
	file_fd.payload = stream;
	stream->fd = fd_add(task_with_pid(getpid()), file_fd);

	return stream;
}

bool dirent_for_start_sector(uint32_t desired_sector, fat_dirent* directory, fat_dirent* store) {
	if (!store) {
		printf("dirent_for_start_sector() no dirent to store in!\n");
		return false;
	}

	uint32_t search_dir = directory->first_sector;
	if (search_dir == desired_sector) {
		printf("dirent_for_start_sector(%d) == search_dir\n", desired_sector);

		strcpy(store->name, "dir");
		store->is_directory = true;

		uint32_t sector_count = 0;
		fat_file_last_sector(desired_sector, &sector_count);
		store->size = sector_count * SECTOR_SIZE;
		store->first_sector = desired_sector;
		return true;
	}

	if (!is_valid_sector(desired_sector)) {
		return false;
	}

	printf("dirent_for_start_sector reached this line\n");
	while (is_valid_sector(search_dir)) {
		printk("iter\n");
		//look through this sector and see if we have any free space
		fat_directory sector_contents;
		fat_read_file(directory, (char*)&sector_contents, sizeof(sector_contents), 0);

		for (uint32_t j = 0; j < sizeof(sector_contents.entries) / sizeof(sector_contents.entries[0]); j++) {
			fat_dirent entry = sector_contents.entries[j];
			//in use entry?
			if (!strlen(entry.name)) continue;

			//is this what we're looking for?
			if (entry.first_sector == desired_sector) {
				memcpy(store, &entry, sizeof(entry));
				return true;
			}

			if (entry.is_directory) {
				printf("dirent_for_start_sector recursing to %s\n", entry.name);
				if (dirent_for_start_sector(desired_sector, &entry, store)) {
					return true;
				}
			}
		}

		//go to next sector in directory contents
		search_dir = fat[search_dir];
	}
	//dir sector was no longer valid,
	//we've ran out of places to look
	return false;
}

#define ROOT_DIR_SIZE 0x2000
void fat_install(unsigned char drive, bool force_format) {
	//check if this drive has already been formatted
	int magic = fat_read_magic();
	if (!force_format && magic == FAT_MAGIC) {
		printf("FAT filesystem has already been formatted\n");	

		strcpy((char*)&root_dir.name, "/");
		root_dir.first_sector = 0;
		root_dir.is_directory = true;
		root_dir.size = ROOT_DIR_SIZE;

		return;
	}

	printf("Formatting FAT filesystem for first run/corrupted superblock...\n");
	fat_format_disk(drive);
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

	char zeroes[SECTOR_SIZE];
	memset(zeroes, 0, sizeof(zeroes));
	ide_ata_write(fat_disk, MBR_SECTOR, (uint32_t)zeroes, SECTOR_SIZE, 0);
	ide_ata_write(fat_disk, SUPERBLOCK_SECTOR, (uint32_t)zeroes, SECTOR_SIZE, 0);
	fat_record_superblock(SECTOR_SIZE, sectors);

	//after FAT, place root directory entry
	//4kb
	strcpy((char*)&root_dir.name, "/");
	root_dir.first_sector = fat_file_create(ROOT_DIR_SIZE);
	root_dir.is_directory = true;
	root_dir.size = ROOT_DIR_SIZE;

	fat_dirent usr_dir;
	fat_dir_new_file(&root_dir, "usr", SECTOR_SIZE, true, &usr_dir);
	fat_dirent bin_dir;
	fat_dir_new_file(&root_dir, "bin", SECTOR_SIZE, true, &bin_dir);
	fat_dirent include_dir;
	fat_dir_new_file(&root_dir, "include", SECTOR_SIZE, true, &include_dir);
	fat_dirent lib_dir;
	fat_dir_new_file(&root_dir, "lib", SECTOR_SIZE, true, &lib_dir);

	fat_dirent welcome_txt;
	fat_dir_new_file(&include_dir, "welcome.txt", 0x200, false, &welcome_txt);

	/*
	char buf[SECTOR_SIZE];
	strcpy(buf, "Welcome to axle OS.\n\nThis file is stored on a physical hard drive, retrieved using PIO mode on an ATA drive.\nThe drive is formatted with axle's FAT clone filesystem.\nThis filesystem supports expandable files, as well as directories.\nThere are also reserved directory entry sections to be used for file permissions, access times, etc.\n\nVisit www.github.com/codyd51/axle for this OS's source code.\n");
	fat_write_file(&welcome_txt, buf, sizeof(buf), 0);

	fat_print_directory(&root_dir, 0, true);

	FILE* fp = fopen("/include/welcome.txt", "r");
	printf("fp: %x\n", fp);
	memset(buf, 0, sizeof(buf));
	int read_count = fread(buf, sizeof(char), sizeof(buf), fp);
	//int read_count = fat_fread(buf, sizeof(char), sizeof(buf), fp);
	printf("fp read %d bytes: %s\n", read_count, buf);
	*/
	/*
	char* argv[] = {"/usr/bin/false", NULL};

	int pid = sys_fork();
	if (!pid) {
		execve(argv[0], argv, 0);
	}
	int stat;
	waitpid(pid, &stat, 0);
	printf("IDE binary returned with error code %d\n", stat);
	*/
}

