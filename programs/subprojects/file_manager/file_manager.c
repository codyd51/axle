#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <libutils/assert.h>
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

static void _amc_message_received(amc_message_t* msg) {
	// Copy the source service early
	// If we talk to the ATA driver while handling a message (i.e. to read FAT data),
	// we'll overwrite the message data, as amc messages are always delivered to the same memory area
    char* source_service = strdup(msg->source);

	uint32_t event = amc_msg_u32_get_word(msg, 0);
	//printf("File manager sent event %ld from %s\n", event, source_service);
	if (event == FILE_MANAGER_READ_FILE) {
		file_manager_read_file_request_t* req = (file_manager_read_file_request_t*)&msg->body;
		fs_node_t* desired_file = vfs_find_node_by_path(req->path);
		assert(desired_file, "Failed to find requested file");

		uint32_t file_size = 0;
		uint8_t* file_data = NULL;

		if (desired_file->base.type == FS_NODE_TYPE_INITRD) {
			file_data = initrd_read_file(&desired_file->initrd, &file_size);
		}
		else if (desired_file->base.type == FS_NODE_TYPE_FAT) {
			file_data = fat_read_file(&desired_file->fat, &file_size);
		}
		else {
			assert(false, "Unknown file type");
		}

		uint32_t response_size = sizeof(file_manager_read_file_response_t) + file_size;
		file_manager_read_file_response_t* resp = calloc(1, response_size);
		resp->event = FILE_MANAGER_READ_FILE_RESPONSE;
		resp->file_size = file_size;
		memcpy(resp->file_data, file_data, file_size);
		free(file_data);

		printf("Returning file size 0x%08lx buf 0x%08lx to %s\n", resp->file_size, (uint32_t)resp->file_data, source_service);
		amc_message_send(source_service, resp, response_size);
		free(resp);
	}
	else if (event == FILE_MANAGER_READ_FILE__PARTIAL) {
		file_manager_read_file_partial_request_t* req = (file_manager_read_file_partial_request_t*)&msg->body;
		fs_node_t* desired_file = vfs_find_node_by_path(req->path);
		assert(desired_file, "Failed to find requested file");
		assert(desired_file->base.type == FS_NODE_TYPE_FAT, "Only supported for FAT at the moment");

		uint32_t out_length = 0;
		uint8_t* file_data = fat_read_file_partial(&desired_file->fat, req->offset, req->length, &out_length);

		uint32_t response_size = sizeof(file_manager_read_file_partial_response_t) + out_length;
		file_manager_read_file_partial_response_t* resp = calloc(1, response_size);
		resp->event = FILE_MANAGER_READ_FILE__PARTIAL_RESPONSE;
		resp->data_length = out_length;
		memcpy(resp->file_data, file_data, out_length);
		free(file_data);

		//printf("Returning file size 0x%08lx buf 0x%08lx to %s\n", resp->data_length, (uint32_t)resp->file_data, source_service);
		amc_message_send(source_service, resp, response_size);
		free(resp);
	}
	else if (event == FILE_MANAGER_LAUNCH_FILE) {
		file_manager_launch_file_request_t* req = (file_manager_launch_file_request_t*)&msg->body;
		initrd_fs_node_t* desired_file = vfs_find_node_by_path__initrd(req->path);
		assert(desired_file->base.type == FS_NODE_TYPE_INITRD, "Expected initrd but this is a soft assumption");
		if (desired_file) {
			printf("File Manager launching %s upon request\n", req->path);
			vfs_launch_program_by_node((fs_node_t*)desired_file);
		}
		else {
			printf("Failed to find requested file to launch for %s: %s\n", source_service, req->path);
		}
	}
	else if (event == FILE_MANAGER_CREATE_DIRECTORY) {
		file_manager_create_directory_request_t* req = (file_manager_create_directory_request_t*)&msg->body;

		bool success = vfs_create_directory((char*)req->path);

		uint32_t response_size = sizeof(file_manager_create_directory_response_t);
		file_manager_create_directory_response_t* resp = calloc(1, response_size);
		resp->event = FILE_MANAGER_CREATE_DIRECTORY_RESPONSE;
		resp->success = success;
		printf("File manager responsing to %s\n", source_service);
		amc_message_send(source_service, resp, response_size);
		free(resp);
	}
	else if (event == FILE_MANAGER_CHECK_FILE_EXISTS) {
		file_manager_check_file_exists_request_t* req = (file_manager_check_file_exists_request_t*)&msg->body;
		// Copy the path as we may send and receive other amc messages to read directory data
		char* path = strdup(req->path);

		fs_node_t* node = vfs_find_node_by_path(path);

		file_manager_check_file_exists_response_t resp = {0};
		resp.event = FILE_MANAGER_CHECK_FILE_EXISTS_RESPONSE;
		resp.file_exists = (node != NULL);
		if (resp.file_exists) {
			if (node->base.type == FS_NODE_TYPE_FAT) {
				resp.file_size = node->fat.size;
			}
			else if (node->base.type == FS_NODE_TYPE_INITRD) {
				resp.file_size = node->initrd.size;
			}
			else {
				printf("[FS] Will not provide size for %s as it is a virtual node\n", path);
			}
		}

		snprintf(resp.path, sizeof(resp.path), "%s", path);

		printf("File manager responsing to %s\n", source_service);
		amc_message_send(source_service, &resp, sizeof(resp));
		free(path);
	}
	else {
		assert(false, "Unknown message sent to file manager");
	}

	free(source_service);
}

static void flash_initrd_file_to_hdd(fat_fs_node_t* parent_directory, const char* initrd_name, const char* name, const char* ext) {
	char filename[64];
	snprintf(filename, sizeof(filename), "%s.%s", name, ext);

	char* hdd_path = vfs_path_for_node((fs_node_t*)parent_directory);
	printf("Flashing initrd/%s to %s/%s...\n", initrd_name, hdd_path, filename);

	char initrd_filename[64];
	snprintf(initrd_filename, sizeof(initrd_filename), "/initrd/%s", initrd_name);
	initrd_fs_node_t* initrd_file = vfs_find_node_by_path__initrd(initrd_filename);

	uint32_t file_len = 0;
	uint8_t* file_data = initrd_read_file(initrd_file, &file_len);

	fat_create_file(parent_directory, name, ext, file_len, file_data);

	printf("Finished flashing initrd/%s to %s/%s...\n", initrd_name, hdd_path, filename);
	free(hdd_path);
}

static void doom_install(void) {
	vfs_create_directory("/hdd/doomdata");
	fat_fs_node_t* dir = vfs_find_node_by_path__fat("/hdd/doomdata");
	// Why does doom1.wad parse as doom.wad before rebooting?
	//flash_initrd_file_to_hdd(dir, "doom.wad", "doom", "wad");
	if (!vfs_find_node_by_path("/hdd/doomdata/doom1.wad")) {
		flash_initrd_file_to_hdd(dir, "doom1.wad", "doom1", "wad");
	}
	//flash_initrd_file_to_hdd(dir, "nos4.wad", "nos4", "wad");
	// TODO(PT): Do FAT files work without an extension?
	//flash_initrd_file_to_hdd(dir, "doom", "doom", "run");
	//flash_initrd_file_to_hdd(dir, "origwad.pwd", "origwad", "pwd");
}

int main(int argc, char** argv) {
	amc_register_service(FILE_MANAGER_SERVICE_NAME);

	// Ask the kernel to map in the ramdisk and send us info about it
	amc_msg_u32_1__send(AXLE_CORE_SERVICE_NAME, AMC_FILE_MANAGER_MAP_INITRD);
	amc_message_t* msg;
	amc_message_await__u32_event(AXLE_CORE_SERVICE_NAME, AMC_FILE_MANAGER_MAP_INITRD_RESPONSE, &msg);
	amc_initrd_info_t* initrd_info = (amc_initrd_info_t*)&msg->body;
	printf("Recv'd initrd info at %p\n", initrd_info);

	char* root_path = "/";
	fs_base_node_t* root = fs_node_create__directory(NULL, root_path, strlen(root_path));
	root->type = FS_NODE_TYPE_ROOT;
	vfs__set_root_node(root);

	//fat_format_drive(ATA_DRIVE_MASTER);

	// Initialise vfs from storage
	initrd_parse_from_amc(root, initrd_info);
	//fat_parse_from_disk(root);

	print_fs_tree((fs_node_t*)root, 0);

	//doom_install();

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
