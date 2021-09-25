#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include "ata.h"
#include "fs_node.h"

#define FAT_SECTOR_INDEX__BOOT			0
#define FAT_SECTOR_INDEX__TABLE			1
#define FAT_SECTOR_INDEX__ROOT_DIR		2
#define FAT_SECTOR_INDEX__DATA_START	3

#define FAT_SECTOR_TYPE__EOF	(0)

// TODO(PT): This structure is currently unused
typedef struct fat_boot_sector {
	uint8_t bootstrap[3];
	uint8_t description[8];
	uint16_t sector_size;
	uint8_t sectors_per_allocation_unit;
	uint16_t reserved_blocks;
	uint8_t fat_count;
	uint16_t root_directory_entry_count;
	uint16_t sectors_on_disk;
	uint8_t media_descriptor;
	// Size of table to address the rest of the disk
	uint16_t fat_sector_count;
	uint16_t sectors_per_track;
	uint16_t disk_head_count;
	uint32_t hidden_blocks_count;
	uint32_t sectors_on_disk_ext;
	uint16_t drive_number;
	uint8_t extended_boot_record_signature;
	uint32_t serial_number;
	uint8_t disk_name[11];
	uint8_t filesystem_identifier[8];
	uint8_t bootstrap_ext[0x1c0];
	uint16_t boot_sector_signature;
} fat_boot_sector_t;

typedef struct fat_drive_info {
	uint32_t sector_size;
	uint32_t disk_size_in_bytes;
	// Size of table to address the rest of the disk
	uint32_t fat_sector_count;
	uint32_t fat_head_sector;
	uint32_t root_directory_head_sector;
	uint32_t fat_sector_slide;

	// Computed data
	uint32_t sectors_on_disk;
	uint32_t fat_entries_per_sector;
} fat_drive_info_t;

typedef struct fat_entry {
	bool allocated;
	uint8_t reserved;
	uint16_t next_fat_entry_idx_in_file;
} fat_entry_t;

#define FAT_FILENAME_SIZE 8
#define FAT_FILE_EXT_SIZE 3

typedef struct fat_directory_entry {
	bool is_directory;
	char filename[FAT_FILENAME_SIZE];
	char ext[FAT_FILE_EXT_SIZE];
	uint16_t first_fat_entry_idx_in_file;
	// Not valid for directories
	uint32_t size;
	uint8_t reserved[14];
} __attribute__((packed)) fat_directory_entry_t;

typedef struct fat_directory {
	uint32_t slot_count;
	fat_directory_entry_t* slots;
} fat_directory_t;

typedef struct fat_fs_node {
	fs_base_node_t base;
    // Private fields
	uint32_t first_fat_entry_idx_in_file;
	uint32_t size;
} fat_fs_node_t;

void fat_format_drive(ata_drive_t drive);
fat_fs_node_t* fat_parse_from_disk(fs_base_node_t* vfs_root);
fat_drive_info_t fat_drive_info(void);

uint8_t* fat_read_file(fat_fs_node_t* fs_node, uint32_t* out_file_size);
uint8_t* fat_read_file_partial(fat_fs_node_t* fs_node, uint32_t offset, uint32_t length, uint32_t* out_length);

fat_fs_node_t* fat_create_directory(fat_fs_node_t* parent_directory, const char* filename);
fat_fs_node_t* fat_create_file(fat_fs_node_t* parent_directory, const char* filename, const char* ext, uint32_t file_len, const char* file_data);

#endif