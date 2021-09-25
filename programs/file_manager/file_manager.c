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

	fs_base_node_t* initrd_root = initrd_parse_from_amc(root, initrd_info);

	//fat_format_drive(ATA_DRIVE_MASTER);

	fat_fs_node_t* fat_root = fat_parse_from_disk(root);

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
