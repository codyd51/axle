#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlibadd/assert.h>

#include "vfs.h"

#include "tests.h"

void tests_init(void) {
	if (!vfs_find_node_by_path__fat("/hdd/tests")) {
		vfs_create_node("/hdd/tests", VFS_NODE_TYPE_DIRECTORY);
	}
}

void test_create_file(void) {
	const char* path = "/hdd/tests/file.abc";
	bool created = vfs_create_node(path, VFS_NODE_TYPE_FILE);
	assert(created, "Expected file to be created now");
	fat_fs_node_t* node = vfs_find_node_by_path__fat(path);
	assert(node != NULL, "Failed to find node?");
	assert(node->size == 0, "Expected zero size");
	assert(node->first_fat_entry_idx_in_file == FAT_SECTOR_TYPE__EOF, "Expected no start sector");
}

void test_file_read_partial(void) {
	uint8_t* data = calloc(1, 600);

	// Write some data across a sector boundary
	memset(data, 'A', 512);
	memset(data+512, 'B', 88);

	fat_fs_node_t* node = vfs_find_node_by_path__fat("/hdd/test.txt");
	if (!node) {
		printf("Creating test file...\n");
		fat_create_file(fat_mount_point(), "test", "txt", 600, data);
		node = vfs_find_node_by_path__fat("/hdd/test.txt");
		assert(node != NULL, "Failed to create test file!");
	}

	// Given I request a read larger than the file size
	uint32_t out_len = 0;
	uint8_t* read_data = fat_read_file_partial(node, 0, 1000, &out_len);
	// Then only the bytes present in the file are read
	assert(out_len == 600, "File should only have 600 bytes");
	// And the file data is read correctly
	assert(memcmp(read_data, data, out_len) == 0, "File data differed");
	free(read_data);

	// Given I request a read across a sector boundary
	read_data = fat_read_file_partial(node, 510, 4, &out_len);
	// Then exactly the requested number of bytes is read
	assert(out_len == 4, "Only wanted 4 bytes!");
	// And the file data is read correctly
	assert(memcmp(read_data, data + 510, out_len) == 0, "File data differed");
	free(read_data);

	// Given I request a read across after a sector boundary
	read_data = fat_read_file_partial(node, 575, 35, &out_len);
	// Then exactly the requested number of bytes is read
	// Then the file data is capped at the file length
	assert(out_len == 25, "Only 25 bytes were available");
	// And the file data is read correctly
	assert(memcmp(read_data, data + 550, out_len) == 0, "File data differed");
	free(read_data);

	// Given I request a read at the EOF
	read_data = fat_read_file_partial(node, 600, 10, &out_len);
	// Then no bytes are read
	assert(out_len == 0, "At EOF - Expected no data to be read");
	// And no output buffer is provided
	assert(read_data == NULL, "At EOF - Expected no data to be read");

	// Given I request a read past the EOF
	read_data = fat_read_file_partial(node, 1000, 10, &out_len);
	// Then no bytes are read
	assert(out_len == 0, "Past EOF - Expected no data to be read");
	// And no output buffer is provided
	assert(read_data == NULL, "Past EOF - Expected no data to be read");

	// Given I request a read of zero-length
	read_data = fat_read_file_partial(node, 0, 0, &out_len);
	// Then no bytes are read
	assert(out_len == 0, "Zero-length read: Expected no data to be read");
	// And no output buffer is provided
	assert(read_data == NULL, "Zero-length read: Expected no data to be read");

	free(data);
}
