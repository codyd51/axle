#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libutils/assert.h>
#include <libutils/sleep.h>

#include "ata.h"
#include "vfs.h"
#include "math.h"
#include "fat.h"

void fat_cache_invalidate_table_sector(uint32_t fat_table_sector_idx);

static fat_drive_info_t _g_fat_drive_info = {0};

fat_drive_info_t fat_drive_info(void) {
	return _g_fat_drive_info;
}

void fat_format_drive(ata_drive_t drive) {
	// Validate size assumptions
	assert(sizeof(fat_entry_t) == sizeof(uint32_t), "Expected a FAT entry to occupy exactly 4 bytes");
	assert(sizeof(fat_directory_entry_t) == 32, "Expected a FAT directory entry to occupy exactly 32 bytes");

	printf("[FAT] Formatting drive %d...\n", drive);

	// TODO(PT): Pull disk size & sector size from ATA driver. For now, assume 4MB
	fat_drive_info_t* drive_info = &_g_fat_drive_info;
	drive_info->sector_size = 512;
	drive_info->disk_size_in_bytes = 64 * 1024 * 1024;
	drive_info->sectors_on_disk = drive_info->disk_size_in_bytes / drive_info->sector_size;
	printf("[FAT] Sectors on disk: %ld\n", drive_info->sectors_on_disk);

	// https://www.keil.com/pack/doc/mw/FileSystem/html/fat_fs.html
	// Format the boot sector
	// Once we boot from disk, this will contain the MBR / startup code
	// For now, write some dummy data so it's clear what's going on
	uint8_t* boot_sector_data = calloc(1, drive_info->sector_size);
	memset(boot_sector_data, 'A', drive_info->sector_size);
	ata_write_sector(FAT_SECTOR_INDEX__BOOT, boot_sector_data);
	free(boot_sector_data);

	// Format the FAT table
	drive_info->fat_entries_per_sector = drive_info->sector_size / sizeof(fat_entry_t);
	printf("[FAT] Entries per sector: %ld\n", drive_info->fat_entries_per_sector);

	// How many sectors will we need to address the whole disk?
	uint32_t bytes_tracked_per_fat_sector = drive_info->sector_size * drive_info->fat_entries_per_sector;
	drive_info->fat_sector_count = drive_info->disk_size_in_bytes / bytes_tracked_per_fat_sector;

	printf("[FAT] Bytes tracked per FAT sector: %ld\n", bytes_tracked_per_fat_sector);
	printf("[FAT] Sectors needed to address disk: %ld\n", drive_info->fat_sector_count);
	printf("[FAT] Disk size: %ldMB\n", drive_info->disk_size_in_bytes / 1024 / 1024);

	// Subtract the sectors we need to store the FAT itself, plus the boot sector
	// Technically we might be able to reclaim some sectors that won't need to be tracked since they'll be 
	// allocated to the FAT, but I didn't bother with this for now.
	uint32_t sectors_tracked_by_fat = drive_info->sectors_on_disk - (drive_info->fat_sector_count + 1);
	printf("[FAT] Sectors tracked by FAT: %ld (%.2f MB)\n", sectors_tracked_by_fat, (sectors_tracked_by_fat * drive_info->sector_size) / 1024.0 / 1024.0);

	// Format the FAT sectors (starting at sector 1, after the boot sector)
	drive_info->fat_head_sector = 1;
	for (uint32_t i = 0; i < drive_info->fat_sector_count; i++) {
		uint32_t sector_idx = drive_info->fat_head_sector + i;
		fat_entry_t* fat_data = calloc(1, drive_info->fat_entries_per_sector * sizeof(fat_entry_t));
		for (uint32_t j = 0; j < drive_info->fat_entries_per_sector; j++) {
			fat_data[j].allocated = false;
			fat_data[j].reserved = 0;
			fat_data[j].next_fat_entry_idx_in_file = FAT_SECTOR_TYPE__EOF;
		}
		ata_write_sector(sector_idx, fat_data);
	}
	printf("[FAT] Finished writing %ld FAT tables\n", drive_info->fat_sector_count);

	// Sectors tracked by FAT are offset by the boot sector and FAT itself
	drive_info->fat_sector_slide = drive_info->fat_head_sector + drive_info->fat_sector_count;

	// Instantiate the root directory
	drive_info->root_directory_head_sector = drive_info->fat_sector_slide;
	printf("[FAT] Allocating root directory in sector %ld\n", drive_info->root_directory_head_sector);

	uint32_t directory_entries_per_directory_sector = drive_info->sector_size / sizeof(fat_directory_entry_t);
	fat_directory_entry_t* root_directory = calloc(1, directory_entries_per_directory_sector * sizeof(fat_directory_entry_t));
	for (uint32_t i = 0; i < directory_entries_per_directory_sector; i++) {
		root_directory[i].is_directory = false;
		root_directory[i].first_fat_entry_idx_in_file = 0;
	}
	ata_write_sector(drive_info->root_directory_head_sector, root_directory);
	free(root_directory);

	// And place the root directory in the FAT
	ata_sector_t* fat_sector = ata_read_sector(drive_info->fat_head_sector);
	fat_entry_t* fat_sector_data = (fat_entry_t*)fat_sector->data;
	fat_sector_data[0].allocated = true;
	fat_sector_data[0].next_fat_entry_idx_in_file = FAT_SECTOR_TYPE__EOF;
	ata_write_sector(drive_info->fat_head_sector, fat_sector_data);
	free(fat_sector);
	printf("[FAT] Finished formatting disk\n");
}

typedef struct fat_entry_descriptor {
	uint32_t fat_entry_idx;
	fat_entry_t ent;
} fat_entry_descriptor_t;

void fat_alloc_sector(fat_drive_info_t drive_info, uint32_t next_fat_entry_idx_in_file, fat_entry_descriptor_t* out_desc) {
	static int last_sector_with_free_space = 1;
	for (uint32_t i = 0; i < drive_info.fat_sector_count; i++) {
		uint32_t sector_index = drive_info.fat_head_sector + i;
		if (sector_index < last_sector_with_free_space) {
			continue;
		}
		ata_sector_t* fat_sector = ata_read_sector(sector_index);
		fat_entry_t* fat_sector_data = (fat_entry_t*)fat_sector->data;

		for (uint32_t j = 0; j < drive_info.fat_entries_per_sector; j++) {
			if (fat_sector_data[j].allocated == false) {
				// Found a free FAT entry to allocate in
				last_sector_with_free_space = sector_index;

				// Clear the data sector on disk so we don't leak data from deleted files
				uint32_t disk_sector = ((i * drive_info.fat_entries_per_sector) + j) + drive_info.fat_sector_slide;
				uint8_t* zeroes = calloc(1, drive_info.sector_size);
				memset(zeroes, 0, drive_info.sector_size);
				ata_write_sector(disk_sector, zeroes);

				//printf("[FAT] fat_alloc_sector allocating in FAT sector %ld (unslid %ld), index %ld (unslid %ld)\n", i, sector_index, j, disk_sector);
				fat_sector_data[j].allocated = true;
				fat_sector_data[j].next_fat_entry_idx_in_file = next_fat_entry_idx_in_file;
				
				// Write the FAT sector back to disk
				ata_write_sector(sector_index, fat_sector_data);
				free(fat_sector);

				// Retain a copy of the data we've written
				out_desc->fat_entry_idx = (i * drive_info.fat_entries_per_sector) + j;
				out_desc->ent.allocated = true;
				out_desc->ent.next_fat_entry_idx_in_file = next_fat_entry_idx_in_file;

				// Note that our FAT cache has been invalidated for this sector
				// (Note we're using the FAT-relative sector addressing)
				fat_cache_invalidate_table_sector(i);

				return;
			}
		}
		free(fat_sector);
	}
	assert(false, "FAT entirely full!");
}

uint32_t _fat_create_dir_or_file(fat_drive_info_t drive_info, uint32_t directory_fat_entry_index, bool is_directory, const char* filename, const char* ext, uint32_t file_len, const uint8_t* file_data) {
	printf("[FAT] _fat_create_dir_or_file (is dir? %d, directory fat entry idx %ld) %s.%s %ld\n", is_directory, directory_fat_entry_index, filename, ext, file_len);

	// Find the disk sector containing the directory data
	uint32_t directory_sector_index = directory_fat_entry_index + drive_info.fat_sector_slide;
	printf("\tDirectory stored in sector %ld\n", directory_sector_index);

	ata_sector_t* directory_sector = ata_read_sector(directory_sector_index);
	fat_directory_t directory = {
		.slot_count = drive_info.sector_size / sizeof(fat_directory_entry_t),
		.slots = (fat_directory_entry_t*)directory_sector->data
	};

	// Iterate the slots in the directory until we find a free one
	for (uint32_t i = 0; i < directory.slot_count; i++) {
		printf("\tDirectory slot %ld: first entry %d\n", i, directory.slots[i].first_fat_entry_idx_in_file);
		if (directory.slots[i].first_fat_entry_idx_in_file == FAT_SECTOR_TYPE__EOF) {
			printf("\tPlacing file in directory slot #%ld\n", i);

			directory.slots[i].is_directory = is_directory;
			strncpy(directory.slots[i].filename, filename, sizeof(directory.slots[i].filename));

			if (is_directory) {
				file_len = drive_info.sector_size;
			}
			else {
				strncpy(directory.slots[i].ext, ext, sizeof(directory.slots[i].ext));
			}

			directory.slots[i].size = file_len;

			// This algorithm writes the file from back-to-front,
			// allocating the terminating FAT entry in the generated linked-list first,
			// and setting up the links as we go.
			// Writing the FAT links and file data backwards in this way allows us to 
			// write the FAT/file data in a single pass, without needing intermediate storage.
			uint32_t needed_sectors = (file_len / drive_info.sector_size);
			uint32_t file_bytes_in_terminating_sector = drive_info.sector_size;
			if (file_len % drive_info.sector_size) {
				needed_sectors += 1;
				file_bytes_in_terminating_sector = file_len % drive_info.sector_size;
			}
			printf("\tWill need %ld sectors to store %ld bytes\n", needed_sectors, file_len);

			uint32_t next_fat_entry_idx = FAT_SECTOR_TYPE__EOF;

			// Only the last sector may have less data to write than a full sector
			uint32_t bytes_to_write_in_next_sector = file_bytes_in_terminating_sector;
			const uint8_t* file_ptr = (file_data + file_len) - bytes_to_write_in_next_sector;

			for (uint32_t j = 0; j < needed_sectors; j++) {
				fat_entry_descriptor_t out_desc = {0};
				fat_alloc_sector(fat_drive_info(), next_fat_entry_idx, &out_desc);
				next_fat_entry_idx = out_desc.fat_entry_idx;
				//printf("\tAllocated fat entry %ld, next in file: %ld\n", out_desc.fat_entry_idx, out_desc.ent.next_fat_entry_idx_in_file);

				// Write the corresponding file data in the data sector associated with the FAT entry
				uint32_t disk_sector = out_desc.fat_entry_idx + drive_info.fat_sector_slide;
				//printf("\tWriting %ld of file data to sector %ld...\n", bytes_to_write_in_next_sector, disk_sector);

				if (!is_directory) {
					uint8_t* data_to_write = calloc(1, drive_info.sector_size);
					memcpy(data_to_write, file_ptr, bytes_to_write_in_next_sector);
					ata_write_sector(disk_sector, data_to_write);
					free(data_to_write);
					// Only the last sector in the file data (i.e. the first iteration of this loop) 
					// can possibly write less than a full sector of data.
					// Therefore, on future iterations (= earlier parts of the file), we'll always
					// write full sectors.
					bytes_to_write_in_next_sector = drive_info.sector_size;
					// Move the window of file data to write backwards by a sector size
					file_ptr -= bytes_to_write_in_next_sector;
				}
			}

			// Update the directory entry and write it back to disk
			directory.slots[i].first_fat_entry_idx_in_file = next_fat_entry_idx;
			ata_write_sector(directory_sector_index, directory.slots);
			free(directory_sector);
			return directory.slots[i].first_fat_entry_idx_in_file;
		}
	}
	free(directory_sector);
	assert(false, "Failed to find free space in directory to create new file!");
	return 0;
}

fat_fs_node_t* fat_create_directory(fat_fs_node_t* parent_directory, const char* filename) {
	assert(parent_directory->base.type == FS_NODE_TYPE_FAT, "Can only create a FAT directory within another FAT directory");
	uint32_t first_fat_entry_idx = _fat_create_dir_or_file(fat_drive_info(), parent_directory->first_fat_entry_idx_in_file, true, filename, NULL, 0, NULL);

	char fat_filename[32];
	snprintf(fat_filename, sizeof(fat_filename), "%.*s", FAT_FILENAME_SIZE, filename);

	fat_fs_node_t* new_node = (fat_fs_node_t*)fs_node_create__directory((fs_base_node_t*)parent_directory, fat_filename, strlen(fat_filename));
	new_node->base.type = FS_NODE_TYPE_FAT;
	new_node->first_fat_entry_idx_in_file = first_fat_entry_idx;
	// TODO(PT): Change me when directories can be larger
	new_node->size = fat_drive_info().sector_size;
	return new_node;
}

fat_fs_node_t* fat_create_file(fat_fs_node_t* parent_directory, const char* filename, const char* ext, uint32_t file_len, const uint8_t* file_data) {
	assert(parent_directory->base.type == FS_NODE_TYPE_FAT, "Can only create a FAT file within a FAT directory");
	uint32_t first_fat_entry_idx = _fat_create_dir_or_file(fat_drive_info(), parent_directory->first_fat_entry_idx_in_file, false, filename, ext, file_len, file_data);

	char fat_filename[32];
	snprintf(fat_filename, sizeof(fat_filename), "%.*s.%.*s", (int)sizeof(FAT_FILENAME_SIZE), filename, (int)sizeof(FAT_FILE_EXT_SIZE), ext);

	fat_fs_node_t* new_node = (fat_fs_node_t*)fs_node_create__file((fs_base_node_t*)parent_directory, fat_filename, strlen(fat_filename));
	new_node->base.type = FS_NODE_TYPE_FAT;
	new_node->first_fat_entry_idx_in_file = first_fat_entry_idx;
	new_node->size = file_len;
	return new_node;
}

static fat_fs_node_t* _parse_node_from_fat_entry(fs_base_node_t* parent_directory, fat_directory_entry_t* directory_entry) {
	printf("[FAT] _parse_node_from_fat_entry\n");
	char full_filename[32];
	fat_fs_node_t* fs_node = NULL;

	if (directory_entry->is_directory) {
		snprintf(full_filename, sizeof(full_filename), "%.*s", FAT_FILENAME_SIZE, directory_entry->filename);
		fs_node = (fat_fs_node_t*)fs_node_create__directory(parent_directory, full_filename, strlen(full_filename));
	}
	else {
		snprintf(full_filename, sizeof(full_filename), "%.*s.%.*s", FAT_FILENAME_SIZE, directory_entry->filename, FAT_FILE_EXT_SIZE, directory_entry->ext);
		fs_node = (fat_fs_node_t*)fs_node_create__file(parent_directory, full_filename, strlen(full_filename));
	}

	fs_node->base.type = FS_NODE_TYPE_FAT;
	fs_node->first_fat_entry_idx_in_file = directory_entry->first_fat_entry_idx_in_file;
	fs_node->size = directory_entry->size;

	printf("\tParsed node %s within directory %s\n", fs_node->base.name, parent_directory->name);
	return fs_node;
}

static void _parse_fat_directory(fat_fs_node_t* directory) {
	// TODO(PT): Update me when we support multi-sector directories
	printf("[FAT] _parse_fat_directory finding directory sector for fat entry idx %ld\n", directory->first_fat_entry_idx_in_file);
	ata_sector_t* directory_sector = ata_read_sector(fat_drive_info().fat_sector_slide + directory->first_fat_entry_idx_in_file);
	fat_directory_entry_t* directory_entries = (fat_directory_entry_t*)&directory_sector->data;

	printf("[FAT] Directory size: %ld\n", directory->size);
	for (uint32_t i = 0; i < directory->size / sizeof(fat_directory_entry_t); i++) {
		if (directory_entries[i].first_fat_entry_idx_in_file != FAT_SECTOR_TYPE__EOF) {
			fat_fs_node_t* fs_node = _parse_node_from_fat_entry((fs_base_node_t*)directory, &directory_entries[i]);
			if (directory_entries[i].is_directory) {
				printf("Recursing for directory %s...\n", fs_node->base.name);
				_parse_fat_directory(fs_node);
			}
		}
	}

	free(directory_sector);
}

static array_t* _g_fat_sector_cache = NULL;

typedef struct fat_cached_sector {
	bool valid;
	uint8_t data[512];
} fat_cached_sector_t;

fat_fs_node_t* fat_parse_from_disk(fs_base_node_t* vfs_root) {
	// TODO(PT): If the disk has been formatted, read these values from disk
	// Otherwise, format the disk and store them
	_g_fat_drive_info.sector_size = 512;
	_g_fat_drive_info.disk_size_in_bytes = 64 * 1024 * 1024;

	_g_fat_drive_info.sectors_on_disk = _g_fat_drive_info.disk_size_in_bytes / _g_fat_drive_info.sector_size;
	_g_fat_drive_info.fat_entries_per_sector = _g_fat_drive_info.sector_size / sizeof(fat_entry_t);

	uint32_t bytes_tracked_per_fat_sector = _g_fat_drive_info.sector_size * _g_fat_drive_info.fat_entries_per_sector;
	_g_fat_drive_info.fat_sector_count = _g_fat_drive_info.disk_size_in_bytes / bytes_tracked_per_fat_sector;

	_g_fat_drive_info.fat_head_sector = 1;
	// Sectors tracked by FAT are offset by the boot sector and FAT itself
	_g_fat_drive_info.fat_sector_slide = _g_fat_drive_info.fat_head_sector + _g_fat_drive_info.fat_sector_count;
	_g_fat_drive_info.root_directory_head_sector = _g_fat_drive_info.fat_sector_slide;

	printf("[FAT] Parsing root directory...\n");
	ata_sector_t* root_directory_sector = ata_read_sector(fat_drive_info().root_directory_head_sector);
	fat_directory_entry_t* root_directory = (fat_directory_entry_t*)&root_directory_sector->data;
	const char* fat_root_name = "hdd";
	fat_fs_node_t* fat_root = (fat_fs_node_t*)fs_node_create__directory(vfs_root, fat_root_name, strlen(fat_root_name));
	fat_root->base.type = FS_NODE_TYPE_FAT;
	fat_root->size = fat_drive_info().sector_size;
	fat_root->first_fat_entry_idx_in_file = 0;

	for (uint32_t i = 0; i < fat_drive_info().sector_size / sizeof(fat_directory_entry_t); i++) {
		if (root_directory[i].first_fat_entry_idx_in_file != FAT_SECTOR_TYPE__EOF) {
			fat_fs_node_t* fs_node = _parse_node_from_fat_entry((fs_base_node_t*)fat_root, &root_directory[i]);
			if (root_directory[i].is_directory) {
				printf("\tRecursing for directory %s...\n", fs_node->base.name);
				_parse_fat_directory(fs_node);
			}
		}
	}

	free(root_directory_sector);

	_g_fat_sector_cache = array_create(_g_fat_drive_info.fat_sector_count);
	for (uint32_t i = 0; i < _g_fat_drive_info.fat_sector_count; i++) {
		fat_cached_sector_t* desc = calloc(1, sizeof(fat_cached_sector_t));
		desc->valid = false;
		array_insert(_g_fat_sector_cache, desc);
	}

	return fat_root;
}

uint8_t* fat_read_file(fat_fs_node_t* fs_node, uint32_t* out_file_size) {
	printf("Reading FAT file %s, first fat entry idx %ld, size %ld\n", fs_node->base.name, fs_node->first_fat_entry_idx_in_file, fs_node->size);

	uint32_t fat_entry_index = fs_node->first_fat_entry_idx_in_file;
	uint32_t file_offset = 0;
	uint8_t* out_file_data = calloc(1, fs_node->size);

	while (true) {
		uint32_t fat_sector_index = fat_entry_index / fat_drive_info().fat_entries_per_sector;
		ata_sector_t* fat_sector = ata_read_sector(fat_drive_info().fat_head_sector + fat_sector_index);
		fat_entry_t* fat_sector_data = (fat_entry_t*)fat_sector->data;

		uint32_t next_fat_entry_index = fat_sector_data[fat_entry_index % fat_drive_info().fat_entries_per_sector].next_fat_entry_idx_in_file;
		printf("[FS] Followed link from FAT entry index %ld to %ld\n", fat_entry_index, next_fat_entry_index);

		// Read the actual sector
		uint32_t bytes_remaining_in_file = fs_node->size - file_offset;
		uint32_t bytes_to_copy_from_sector = min(fat_drive_info().sector_size, bytes_remaining_in_file);
		ata_sector_t* data_sector = ata_read_sector(fat_drive_info().fat_sector_slide + fat_entry_index);

		printf("\tCopy %ld bytes from file data 0x%08lx to buffer 0x%08lx\n", bytes_to_copy_from_sector, (uint32_t)data_sector->data, (uint32_t)out_file_data + file_offset);
		memcpy(out_file_data + file_offset, &data_sector->data, bytes_to_copy_from_sector);

		file_offset += bytes_to_copy_from_sector;
		fat_entry_index = next_fat_entry_index;

		free(data_sector);
		free(fat_sector);

		if (next_fat_entry_index == FAT_SECTOR_TYPE__EOF) {
			printf("[FS] Found end of file!\n");
			break;
		}
	}
	return out_file_data;
}

void fat_cache_invalidate_table_sector(uint32_t fat_table_sector_idx) {
	fat_cached_sector_t* cached_sector = array_lookup(_g_fat_sector_cache, fat_table_sector_idx);
	if (cached_sector->valid) {
		printf("[FAT] Invalidating cache of table sector %ld\n", fat_table_sector_idx);
		cached_sector->valid = false;
	}
}

fat_entry_t* fat_read_table_sector(uint32_t fat_table_sector_idx) {
	// In cache?
	if (fat_table_sector_idx >= _g_fat_sector_cache->size) {
		assert(false, "fat_read_table_sector passed invalid sector?");
		return NULL;
	}

	fat_cached_sector_t* cached_sector = array_lookup(_g_fat_sector_cache, fat_table_sector_idx);
	if (cached_sector->valid) {
		//printf("[FAT] Returning FAT sector index %ld from cache\n", fat_table_sector_idx);
		return (fat_entry_t*)&cached_sector->data;
	}

	fat_drive_info_t drive_info = fat_drive_info();
	ata_sector_t* fat_sector = ata_read_sector(drive_info.fat_head_sector + fat_table_sector_idx);

	printf("[FAT] Inserting FAT sector index %ld into cache\n", fat_table_sector_idx);
	cached_sector->valid = true;
	memcpy(&cached_sector->data, &fat_sector->data, drive_info.sector_size);

	free(fat_sector);

	return (fat_entry_t*)&cached_sector->data;
}

uint8_t* fat_read_file_partial(fat_fs_node_t* fs_node, uint32_t offset, uint32_t length, uint32_t* out_length) {
	//printf("[FS] fat_read_file_partial(%s, off=%ld, len=%ld)\n", fs_node->base.name, offset, length);
	fat_drive_info_t drive_info = fat_drive_info();
	uint32_t sector_size = drive_info.sector_size;

	uint32_t fat_entry_index = fs_node->first_fat_entry_idx_in_file;
	uint32_t fat_link_index_where_data_begins = offset / sector_size;
	uint32_t curr_fat_link_index = 0;

	// Traverse to the FAT entry where the start of the data to read lives
	while (true) {
		if (curr_fat_link_index == fat_link_index_where_data_begins) {
			//printf("\t[FS] Traversed to FAT link index where data begins: link %ld\n", curr_fat_link_index);
			break;
		}

		// Travel to the next FAT entry index
		uint32_t fat_sector_index = fat_entry_index / drive_info.fat_entries_per_sector;
		fat_entry_t* fat_sector_data = fat_read_table_sector(fat_sector_index);

		uint32_t next_fat_entry_index = fat_sector_data[fat_entry_index % drive_info.fat_entries_per_sector].next_fat_entry_idx_in_file;
		//printf("\t[FS] Followed link from FAT entry index %ld to %ld\n", fat_entry_index, next_fat_entry_index);
		fat_entry_index = next_fat_entry_index;
		curr_fat_link_index += 1;


		if (next_fat_entry_index == FAT_SECTOR_TYPE__EOF) {
			printf("\t[FS] Found end of file before reaching region where data begins!\n");
			*out_length = 0;
			return NULL;
		}
	}

	// We can now begin reading data!
	// For the first sector we read from, we may need to do a partial read
	uint32_t start_read_within_sector_off = offset % sector_size;
	uint32_t start_reading_file_off = (curr_fat_link_index * sector_size) + start_read_within_sector_off;

	uint32_t bytes_read_so_far = 0;
	uint32_t bytes_remaining_to_read = min(fs_node->size - start_read_within_sector_off, length);
	//printf("\t[FS] Will read %ld bytes, offset within sector %ld\n", bytes_remaining_to_read, start_read_within_sector_off);
	uint8_t* out_file_data = calloc(1, bytes_remaining_to_read);

	assert(bytes_remaining_to_read % sector_size == 0, "not implemented");
	// TODO(PT): Handle edge cases here

	while (true) {
		// Read the actual sector
		uint32_t bytes_to_copy_from_sector = min(sector_size - start_read_within_sector_off, bytes_remaining_to_read);

		ata_sector_t* data_sector = ata_read_sector(drive_info.fat_sector_slide + fat_entry_index);
		memcpy(out_file_data + bytes_read_so_far, &((uint8_t*)data_sector->data)[start_read_within_sector_off], bytes_to_copy_from_sector);
		free(data_sector);

		// start_read_within_sector_off only applies to the first sector
		start_read_within_sector_off = 0;

		bytes_read_so_far += bytes_to_copy_from_sector;
		bytes_remaining_to_read -= bytes_to_copy_from_sector;
		if (bytes_remaining_to_read == 0) {
			break;
		}

		// Travel to the next FAT entry index
		uint32_t fat_sector_index = fat_entry_index / drive_info.fat_entries_per_sector;
		fat_entry_t* fat_sector_data = fat_read_table_sector(fat_sector_index);

		uint32_t next_fat_entry_index = fat_sector_data[fat_entry_index % drive_info.fat_entries_per_sector].next_fat_entry_idx_in_file;
		//printf("\t[FS] Followed link from FAT entry index %ld to %ld\n", fat_entry_index, next_fat_entry_index);
		fat_entry_index = next_fat_entry_index;

		if (next_fat_entry_index == FAT_SECTOR_TYPE__EOF) {
			// Reached EOF before reading the requested number of bytes
			// TODO(PT): Test this assumption is correct
			printf("Found EOF, bytes_read_so_far 0x%08x, bytes_remaining_to_read 0x%08x", bytes_read_so_far, bytes_remaining_to_read);
			break;
		}
	}
	//printf("*** finished read\n");
	*out_length = bytes_read_so_far;
	return out_file_data;
}

fat_fs_node_t* fat_mount_point(void) {
	return vfs_find_node_by_path__fat("/hdd/");
}
