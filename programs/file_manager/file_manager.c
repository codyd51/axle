#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <stdlibadd/assert.h>
#include <libamc/libamc.h>
#include <libgui/libgui.h>
#include <awm/awm_messages.h>

#include "ui.h"
#include "vfs.h"
#include "ata.h"
#include "fat.h"
#include "math.h"
#include "util.h"
#include "file_manager_messages.h"

static void _parse_initrd(fs_base_node_t* initrd_root, amc_initrd_info_t* initrd_info) {
	initrd_header_t* header = (initrd_header_t*)initrd_info->initrd_start;
	uint32_t offset = initrd_info->initrd_start + sizeof(initrd_header_t);
	for (uint32_t i = 0; i < header->nfiles; i++) {
		initrd_file_header_t* file_header = (initrd_file_header_t*)offset;

		assert(file_header->magic == HEADER_MAGIC, "Initrd file header magic was wrong");

		initrd_fs_node_t* fs_node = (initrd_fs_node_t*)fs_node_create__file(initrd_root, file_header->name, strlen(file_header->name));
		fs_node->type = FS_NODE_TYPE_INITRD;
		fs_node->initrd_offset = file_header->offset + initrd_info->initrd_start;
		fs_node->size = file_header->length;

		offset += sizeof(initrd_file_header_t);
	}
}

static void _amc_message_received(amc_message_t* msg) {
	// Copy the source service early
	// If we talk to the ATA driver while handling a message (i.e. to read FAT data),
	// we'll overwrite the message data, as amc messages are always delivered to the same memory area
    char* source_service = strdup(msg->source);

	uint32_t event = amc_msg_u32_get_word(msg, 0);
	printf("File manager sent event %ld from %s\n", event, source_service);
	if (event == FILE_MANAGER_READ_FILE) {
		file_manager_read_file_request_t* req = (file_manager_read_file_request_t*)&msg->body;
		fs_node_t* desired_file = vfs_find_node_by_name(req->path);
		assert(desired_file, "Failed to find requested file");

		uint32_t file_size = 0;
		uint32_t response_size = 0;
		uint8_t* response_buffer = NULL;
		file_manager_read_file_response_t* resp = NULL;

		if (desired_file->base.type == FS_NODE_TYPE_INITRD) {
			initrd_fs_node_t* initrd_file = &desired_file->initrd;
			printf("Found file %s, initrd offset: 0x%08lx\n", initrd_file->name, initrd_file->initrd_offset);

			file_size = initrd_file->size;
			response_size = sizeof(file_manager_read_file_response_t) + file_size;
			response_buffer = calloc(1, response_size);
			resp = (file_manager_read_file_response_t*)response_buffer;

			memcpy(&resp->file_data, (uint8_t*)initrd_file->initrd_offset, file_size);
		}
		else if (desired_file->base.type == FS_NODE_TYPE_FAT) {
			fat_fs_node_t* fat_file = &desired_file->fat;
			printf("Found file %s, first fat entry idx %ld, size %ld\n", fat_file->base.name, fat_file->first_fat_entry_idx_in_file, fat_file->size);

			file_size = fat_file->size;
			response_size = sizeof(file_manager_read_file_response_t) + file_size;
			response_buffer = calloc(1, response_size);
			resp = (file_manager_read_file_response_t*)response_buffer;
			
			uint32_t file_bytes_remaining = file_size;
			uint32_t fat_entry_index = fat_file->first_fat_entry_idx_in_file;
			uint32_t file_offset = 0;
			while (true) {
				uint32_t fat_sector_index = fat_entry_index / fat_drive_info().fat_entries_per_sector;
				ata_sector_t* fat_sector = ata_read_sector(fat_drive_info().fat_head_sector + fat_sector_index);
				fat_entry_t* fat_sector_data = (fat_entry_t*)fat_sector->data;

				uint32_t next_fat_entry_index = fat_sector_data[fat_entry_index % fat_drive_info().fat_entries_per_sector].next_fat_entry_idx_in_file;
				printf("[FS] Followed link from FAT entry index %ld to %ld\n", fat_entry_index, next_fat_entry_index);

				// Read the actual sector
				uint32_t bytes_to_copy_from_sector = min(fat_drive_info().sector_size, file_bytes_remaining);
				file_bytes_remaining -= bytes_to_copy_from_sector;
				ata_sector_t* data_sector = ata_read_sector(fat_drive_info().fat_sector_slide + fat_entry_index);
				printf("copying %ld bytes from 0x%08lx to 0x%08lx, offset %ld\n", bytes_to_copy_from_sector, &data_sector->data, response_buffer + file_offset, file_offset);
				memcpy(response_buffer + file_offset, &data_sector->data, bytes_to_copy_from_sector);
				file_offset += bytes_to_copy_from_sector;

				free(data_sector);
				free(fat_sector);

				if (next_fat_entry_index == FAT_SECTOR_TYPE__EOF) {
					printf("[FS] Found end of file!\n");
					break;
				}

				fat_entry_index = next_fat_entry_index;
			}
		}
		else {
			assert(false, "Unknown file type");
		}

		resp->event = FILE_MANAGER_READ_FILE_RESPONSE;
		resp->file_size = file_size;
		printf("Returning file size 0x%08lx buf 0x%08lx to %s, %s\n", resp->file_size, resp->file_data, source_service, response_buffer);
		amc_message_construct_and_send(source_service, resp, response_size);
		free(response_buffer);
	}
	else if (event == FILE_MANAGER_LAUNCH_FILE) {
		file_manager_launch_file_request_t* req = (file_manager_launch_file_request_t*)&msg->body;
		initrd_fs_node_t* desired_file = (initrd_fs_node_t*)vfs_find_node_by_name(req->path);
		assert(desired_file->type == FS_NODE_TYPE_INITRD, "Expected initrd but this is a soft assumption");
		if (desired_file) {
			printf("File Manager launching %s upon request\n", req->path);
			vfs_launch_program_by_node((fs_node_t*)desired_file);
		}
		else {
			printf("Failed to find requested file to launch for %s: %s\n", source_service, req->path);
		}
	}
	else {
		assert(false, "Unknown message sent to file manager");
	}

	free(source_service);
}

int main(int argc, char** argv) {
	amc_register_service(FILE_MANAGER_SERVICE_NAME);

	// Ask the kernel to map in the ramdisk and send us info about it
	amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_FILE_MANAGER_MAP_INITRD);
	amc_message_t* msg;
	amc_message_await(AXLE_CORE_SERVICE_NAME, &msg);
	uint32_t event = amc_msg_u32_get_word(msg, 0);
	assert(event == AMC_FILE_MANAGER_MAP_INITRD_RESPONSE, "Expected initrd info");
	amc_initrd_info_t* initrd_info = (amc_initrd_info_t*)msg->body;
	printf("Recv'd initrd info!\n");
	printf("0x%08lx 0x%08lx (0x%08lx bytes)\n", initrd_info->initrd_start, initrd_info->initrd_end, initrd_info->initrd_size);

	char* root_path = "/";
	fs_base_node_t* root = fs_node_create__directory(NULL, root_path, strlen(root_path));
	root->type = FS_NODE_TYPE_ROOT;
	vfs__set_root_node(root);

	char* initrd_path = "initrd";
	fs_base_node_t* initrd_root = fs_node_create__directory(root, initrd_path, strlen(initrd_path));
	_parse_initrd(initrd_root, initrd_info);

	//fat_format_drive(ATA_DRIVE_MASTER, &fat_drive_info);
	fat_parse_from_disk(root);

	print_fs_tree((fs_node_t*)root, 0);

	file_manager_load_images();

	gui_window_t* window = gui_window_create("File Manager", 400, 600);
	gui_scroll_view_t* content_view = gui_scroll_view_create(
		window, 
		(gui_window_resized_cb_t)ui_content_view_sizer
	);
	content_view->base.background_color = color_white();
	// TODO(PT): Traverse the tree beforehand to build the depth listing, 
	// then generate the UI tree without having to traverse the node tree each time
	ui_generate_tree((gui_view_t*)content_view, NULL, 0, (fs_node_t*)root);

	gui_add_message_handler(_amc_message_received);
	amc_msg_u32_1__send(AWM_SERVICE_NAME, FILE_MANAGER_READY);

	gui_enter_event_loop();

	return 0;
}
