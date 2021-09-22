#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#include <agx/font/font.h>
#include <agx/lib/shapes.h>
#include <agx/lib/rect.h>
#include <libgui/libgui.h>
#include <libamc/libamc.h>
#include <stdlibadd/assert.h>

#include <image_viewer/image_viewer_messages.h>
#include <awm/awm_messages.h>
#include <libimg/libimg.h>

#include "vfs.h"
#include "ata.h"
#include "fat.h"
#include "file_manager_messages.h"

typedef struct file_view {
    // Private union members (must be first in the structure)
    gui_mouse_entered_cb_t _priv_mouse_entered_cb;
    gui_mouse_exited_cb_t _priv_mouse_exited_cb;
    gui_mouse_moved_cb_t _priv_mouse_moved_cb;
    gui_mouse_dragged_cb_t _priv_mouse_dragged_cb;
    gui_mouse_left_click_cb_t _priv_mouse_left_click_cb;
    gui_mouse_left_click_ended_cb_t _priv_mouse_left_click_ended_cb;
    gui_mouse_scrolled_cb_t _priv_mouse_scrolled_cb;
    gui_key_down_cb_t _priv_key_down_cb;
    gui_key_up_cb_t _priv_key_up_cb;
    gui_draw_cb_t _priv_draw_cb;
    _priv_gui_window_resized_cb_t _priv_window_resized_cb;
    bool _priv_needs_display;

    // Public and shared in union
    uint32_t type;
	Rect frame;
    gui_window_resized_cb_t sizer_cb;
    gui_window_t* window;

    // Public fields
    gui_layer_t* content_layer;
    Rect content_layer_frame;
    bool controls_content_layer;
    Color background_color;
    
    // Callbacks
    gui_mouse_entered_cb_t mouse_entered_cb;
    gui_mouse_exited_cb_t mouse_exited_cb;
    gui_mouse_moved_cb_t mouse_moved_cb;
    gui_mouse_dragged_cb_t mouse_dragged_cb;
    gui_mouse_left_click_cb_t left_click_cb;
    gui_key_down_cb_t key_down_cb;
    gui_key_up_cb_t key_up_cb;
    gui_window_resized_cb_t window_resized_cb;
	gui_teardown_cb_t teardown_cb;

    // Private fields
    array_t* subviews;
    gui_view_t* superview;
    uint32_t border_margin;
    uint32_t title_bar_height;
    char* _title;
    Rect _title_inset;
    gui_layer_t* parent_layer;
    gui_view_elem_for_mouse_pos_cb_t elem_for_mouse_pos_cb;
    gui_draw_cb_t _fill_background_cb;

	// File-view-specific fields
	fs_node_t* fs_node;
	Point parent_folder_start;
	uint32_t idx_within_folder;
	uint32_t dfs_index;
	image_t* icon;
} file_view_t;

static fs_node_t* root_fs_node = NULL;
static image_t* _g_folder_icon = NULL;
static image_t* _g_image_icon = NULL;
static image_t* _g_executable_icon = NULL;
static image_t* _g_text_icon = NULL;

static fat_drive_info_t fat_drive_info = {0};

static const char* _g_image_extensions[] = {".bmp", ".jpg", ".jpeg", NULL};

static fs_node_t* _find_node_by_name(char* name);
bool str_ends_with(char* str, char* suffix);
bool str_ends_with_any(char* str, char* suffixes[]);

static void _draw_string(gui_layer_t* layer, char* text, Point origin, Size font_size, Color color) {
	Point cursor = origin;
	for (uint32_t i = 0; i < strlen(text); i++) {
		gui_layer_draw_char(
			layer,
			text[i],
			cursor.x,
			cursor.y,
			color,
			font_size
		);
		cursor.x += font_size.width;
	}
}

static void _draw_centered_string(gui_layer_t* layer, char* text, Point center, Size font_size, Color color) {
	uint32_t msg_len = strlen(text);
	uint32_t msg_width = msg_len * font_size.width;
	Point origin = point_make(
		center.x - (msg_width / 2.0),
		center.y - (font_size.height / 2.0)
	);
	_draw_string(layer, text, origin, font_size, color);
}

static file_view_t* _file_view_alloc(const char* filename) {
	file_view_t* f = calloc(1, sizeof(file_view_t));
	gui_view_alloc_dynamic_fields((gui_view_t*)f);
	return f;
}

static Rect _content_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static fs_base_node_t* fs_node_create__directory(fs_base_node_t* parent, char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* dir = calloc(1, sizeof(fs_node_t));
	dir->type = FS_NODE_TYPE_BASE;
	dir->is_directory = true;
	dir->children = array_create(64);

	dir->parent = parent;
	if (parent) {
		array_insert(parent->children, dir);
	}

	strncpy(dir->name, name, name_len);
	return dir;
}

static fs_base_node_t* fs_node_create__file(fs_base_node_t* parent, char* name, uint32_t name_len) {
	// Intentionally use the size of the larger union instead of the base structure
	fs_base_node_t* file = calloc(1, sizeof(fs_node_t));
	file->type = FS_NODE_TYPE_BASE;
	file->is_directory = false;
	file->children = NULL;

	file->parent = parent;
	if (parent) {
		array_insert(parent->children, file);
	}

	strncpy(file->name, name, name_len);
	return file;
}

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
	ata_sector_t* directory_sector = ata_read_sector(fat_drive_info.fat_sector_slide + directory->first_fat_entry_idx_in_file);
	fat_directory_entry_t* directory_entries = &directory_sector->data;

	printf("[FAT] Directory size: %ld\n", directory->size);
	for (uint32_t i = 0; i < directory->size / sizeof(fat_directory_entry_t); i++) {
		if (directory_entries[i].first_fat_entry_idx_in_file != FAT_SECTOR_TYPE__EOF) {
			fat_fs_node_t* fs_node = _parse_node_from_fat_entry(directory, &directory_entries[i]);
			if (directory_entries[i].is_directory) {
				printf("Recursing for directory %s...\n", fs_node->base.name);
				_parse_fat_directory(fs_node);
			}
		}
	}

	free(directory_sector);
}

static fat_fs_node_t* _parse_fat(fs_base_node_t* fat_virt_root, fat_drive_info_t drive_info) {
	printf("[FAT] Parsing root directory...\n");
	ata_sector_t* root_directory_sector = ata_read_sector(drive_info.root_directory_head_sector);
	fat_directory_entry_t* root_directory = &root_directory_sector->data;
	const char* fat_root_name = "hdd";
	fat_fs_node_t* fat_root = (fat_fs_node_t*)fs_node_create__directory(fat_virt_root, fat_root_name, strlen(fat_root_name));
	fat_root->base.type = FS_NODE_TYPE_FAT;
	fat_root->size = drive_info.sector_size;
	fat_root->first_fat_entry_idx_in_file = 0;

	for (uint32_t i = 0; i < drive_info.sector_size / sizeof(fat_directory_entry_t); i++) {
		if (root_directory[i].first_fat_entry_idx_in_file != FAT_SECTOR_TYPE__EOF) {
			fat_fs_node_t* fs_node = _parse_node_from_fat_entry(fat_root, &root_directory[i]);
			if (root_directory[i].is_directory) {
				printf("\tRecursing for directory %s...\n", fs_node->base.name);
				_parse_fat_directory(fs_node);
			}
		}
	}

	free(root_directory_sector);
	return fat_root;
}

static void _print_tabs(uint32_t count) {
	for (uint32_t i = 0; i < count; i++) {
		putchar('\t');
	}
}

static void _print_fs_tree(fs_node_t* node, uint32_t depth) {
	_print_tabs(depth);
	const char* type = node->base.is_directory ? "Dir" : "File";
	printf("<%s %s", type, node->base.name);
	if (node->base.type == FS_NODE_TYPE_INITRD) {
		printf(", Start = 0x%08x, Len = 0x%08x>\n", node->initrd.initrd_offset, node->initrd.size);
	}
	else if (node->base.type == FS_NODE_TYPE_FAT) {
		printf(", FirstSector %ld>\n", node->fat.first_fat_entry_idx_in_file);
	}
	else if (node->base.type == FS_NODE_TYPE_ROOT) {
		printf(" (Root)>\n");
	}
	else if (node->base.type == FS_NODE_TYPE_BASE) {
		printf(">\n");
	}
	else {
		printf("Node type: %ld\n", node->base.type);
		assert(false, "Unknown fs node type");
	}

	if (node->base.children) {
		for (uint32_t i = 0; i < node->base.children->size; i++) {
			fs_node_t* child = array_lookup(node->base.children, i);
			_print_fs_tree(child, depth + 1);
		}
	}
}

static uint32_t _depth_first_search__idx(fs_base_node_t* parent, fs_base_node_t* find, uint32_t sum, bool* out_found) {
	sum += 1;
	if (parent == find) {
		*out_found = true;
		return sum;
	}

	if (parent->children) {
		for (uint32_t i = 0; i < parent->children->size; i++) {
			fs_base_node_t* child = array_lookup(parent->children, i);
			sum = _depth_first_search__idx(child, find, sum, out_found);

			if (*out_found) {
				return sum;
			}
		}
	}
	return sum;
}

static Rect _file_view_sizer(file_view_t* view, Size window_size) {
	Size max_icon_size = size_make(32, 32);
	Size icon_size = max_icon_size;
	if (view->icon) {
		icon_size = view->icon->size;
		/*
		if (icon_size.height > max_icon_size.height) {
			float ratio = max_icon_size.height / (float)icon_size.height;
			icon_size.height = max_icon_size.height;
			icon_size.width *= ratio;
		}
		*/
	}
	uint32_t padding_y = 10;

	Point origin = view->parent_folder_start;
	if (view->fs_node->base.type == FS_NODE_TYPE_ROOT) {
		origin = view->superview->content_layer_frame.origin;
	}
	else {
		// Indent from the parent
		// Always indent by the same amount regardless of image dimensions
		origin.x += 30;
		// Indent from previous siblings within this directory
		origin.y = (view->dfs_index) * (max_icon_size.height + padding_y);
	}

	return rect_make(
		origin,
		icon_size
	);
}

static void _file_view_draw(file_view_t* view, bool is_active) {
	Color bg_color = is_active ? color_blue() : color_white();
	Size font_size = size_make(8, 12);

	Rect icon_frame = rect_make(
		view->frame.origin,
		size_make(
			view->frame.size.width, 
			view->frame.size.height /*- (font_size.height * 1)*/
		)
	);

	// Render the appropriate icon for the file
	if (view->icon) {
		/*
		Rect icon_image_frame = rect_make(
			point_make(
				icon_frame.origin.x + 2,
				icon_frame.origin.y + 2
			),
			size_make(
				icon_frame.size.width - 4,
				icon_frame.size.height - 4
			)
		);
		*/
		image_render_to_layer(view->icon, view->parent_layer->scroll_layer.inner, icon_frame);
	}

	// Draw an outline around the icon
	if (is_active) {
		gui_layer_draw_rect(
			view->parent_layer,
			icon_frame,
			bg_color,
			2
		);
	}

	// Draw a label of the file name
	uint32_t label_inset = max(icon_frame.size.width, 40);
	Rect label_frame = rect_make(
		point_make(
			rect_min_x(icon_frame) + label_inset,
			rect_min_y(icon_frame)
		),
		icon_frame.size
	);
	_draw_string(
		view->parent_layer, 
		view->fs_node->base.name,
		point_make(
			rect_min_x(label_frame) + font_size.width,
			rect_mid_y(label_frame) - (font_size.height / 2.0)
		),
		font_size,
		color_black()
	);

	uint32_t tree_indent = 10;

	// Connecting line to the tree structure
	// Horizontal line
	gui_layer_draw_line(
		view->parent_layer,
		line_make(
			point_make(
				view->parent_folder_start.x + tree_indent,
				rect_mid_y(icon_frame)
			),
			point_make(
				rect_min_x(view->frame) - 2,
				rect_mid_y(icon_frame)
			)
		),
		color_light_gray(),
		2
	);
	// Vertical line
	gui_layer_draw_line(
		view->parent_layer,
		line_make(
			point_make(
				view->parent_folder_start.x + tree_indent,
				view->parent_folder_start.y + (view->frame.size.height)
			),
			point_make(
				view->parent_folder_start.x + tree_indent,
				rect_mid_y(icon_frame)
			)
		),
		color_light_gray(),
		1
	);
}

bool str_ends_with(char* str, char* suffix) {
	str = strchr(str, '.');
	if (str) {
		return !strcmp(str, suffix);
	}
	return false;
}

bool str_ends_with_any(char* str, char* suffixes[]) {
	for (uint32_t i = 0; i < 32; i++) {
		if (suffixes[i] == NULL) {
			return false;
		}
		if (str_ends_with(str, suffixes[i])) {
			return true;
		}
	}
	assert(false, "Failed to find NULL terminator");
	return false;
}

static void _launch_program_by_node(fs_node_t* node) {
	assert(node->base.type == FS_NODE_TYPE_INITRD, "Can only launch initrd programs");
	amc_exec_buffer_cmd_t cmd = {0};
	cmd.event = AMC_FILE_MANAGER_EXEC_BUFFER;
	cmd.program_name = node->initrd.name;
	cmd.buffer_addr = (void*)node->initrd.initrd_offset;
	cmd.buffer_size = node->initrd.size;
	amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &cmd, sizeof(amc_exec_buffer_cmd_t));
}

static void _launch_amc_service_if_necessary(const char* service_name) {
	if (amc_service_is_active(service_name)) {
		printf("Will not launch %s because it's already active!\n");
		return;
	}

	const char* program_name = NULL;
	if (!strncmp(service_name, IMAGE_VIEWER_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
		program_name = "image_viewer";
	}
	else {
		assert(false, "Unknown service name");
	}

	initrd_fs_node_t* node = _find_node_by_name(program_name);
	assert(node != NULL, "Failed to find FS node");
	_launch_program_by_node(node);
}

static void _file_view_left_click(file_view_t* view, Point mouse_point) {
	// Don't try to open virtual fs nodes
	if (view->fs_node->base.type != FS_NODE_TYPE_INITRD) {
		printf("Ignoring click on virtual fs node: %s\n", view->fs_node->base.name);
		return;
	}

	// For image files, ask the image viewer to open them
	char* file_name = view->fs_node->initrd.name;
	printf("File name %s\n", file_name);
	if (str_ends_with_any(file_name, _g_image_extensions)) {
		_launch_amc_service_if_necessary(IMAGE_VIEWER_SERVICE_NAME);

		image_viewer_load_image_request_t req = {0};
		req.event = IMAGE_VIEWER_LOAD_IMAGE;
		snprintf(&req.path, sizeof(req.path), "%s", file_name);
		amc_message_construct_and_send(IMAGE_VIEWER_SERVICE_NAME, &req, sizeof(image_viewer_load_image_request_t));
	}
	else if (str_ends_with(file_name, ".txt")) {
		printf("Ignoring click on text file until text viewer is available: %s\n", file_name);
	}
	else {
		_launch_program_by_node(view->fs_node);
	}
}

static void _generate_ui_tree(gui_view_t* container_view, file_view_t* parent_view, uint32_t idx_within_parent, fs_node_t* node) {
	const char* type = node->base.is_directory ? "Dir" : "File";

	file_view_t* file_view = _file_view_alloc(node->base.name);
	file_view->fs_node = node;
	if (parent_view) {
		file_view->parent_folder_start = parent_view->frame.origin;
	}
	file_view->idx_within_folder = idx_within_parent;

	gui_view_init((gui_view_t*)file_view, container_view->window, (gui_window_resized_cb_t)_file_view_sizer);

	// Set up the DFS index before adding it as a subview, 
	// so that it will be positioned correctly on the first draw
	if (node->base.type != FS_NODE_TYPE_ROOT) {
		fs_node_t* root = (fs_node_t*)node->base.parent;
		while (root->base.type != FS_NODE_TYPE_ROOT) {
			root = (fs_node_t*)root->base.parent;
		}
		bool found = false;
		file_view->dfs_index = _depth_first_search__idx((fs_base_node_t*)root, (fs_base_node_t*)node, 0, &found);
		printf("Depth of %s: %d\n", node->base.name, file_view->dfs_index);
	}

	if (node->base.children && node->base.children->size) {
		// Directory
		file_view->icon = _g_folder_icon;
	}
	else if (str_ends_with_any(node->base.name, _g_image_extensions)) {
		// Image
		file_view->icon = _g_image_icon;
	}
	else if (str_ends_with(node->base.name, ".txt")) {
		// Text
		file_view->icon = _g_text_icon;
	}
	else {
		// Executable
		file_view->icon = _g_executable_icon;
	}

	gui_view_add_subview(container_view, (gui_view_t*)file_view);

	file_view->parent_layer = container_view->content_layer;
	file_view->_priv_draw_cb = (gui_draw_cb_t)_file_view_draw;
	file_view->left_click_cb = (gui_mouse_left_click_cb_t)_file_view_left_click;

	if (node->base.children) {
		for (uint32_t i = 0; i < node->base.children->size; i++) {
			fs_node_t* child = array_lookup(node->base.children, i);
			_generate_ui_tree(container_view, file_view, i, child);
		}
	}
}

static fs_node_t* _find_node_by_name(char* name) {
	printf("Look for name %s\n", name);
	// TODO(PT): Change to recursive approach to search all directories
	for (uint32_t i = 0; i < root_fs_node->base.children->size; i++) {
		uint32_t max_path_len = 128;
		char* path = calloc(1, max_path_len);
		fs_base_node_t* node = array_lookup(root_fs_node->base.children, i);
		printf("got child node %s\n", node->name);
		snprintf(path, max_path_len, "/%s", node->name);

		printf("Check if root %s matches %s\n", path, name);
		if (!strncmp(path, name, max_path_len)) {
			free(path);
			return node;
		}
		
		if (node->is_directory) {
			for (uint32_t j = 0; j < node->children->size; j++) {
				fs_base_node_t* child = array_lookup(node->children, j);
				char* appended_path = calloc(1, max_path_len);
				snprintf(appended_path, max_path_len, "%s/%s", path, child->name);
				//printf("Check if child %s matches %s\n", appended_path, name);
				if (!strncmp(appended_path, name, max_path_len)) {
					free(path);
					free(appended_path);
					printf("returning node\n");
					return (fs_node_t*)child;
				}

				free(appended_path);
			}
		}

		free(path);
	}
	return NULL;
}

static void _amc_message_received(amc_message_t* msg) {
	// Copy the source service early
	// If we talk to the ATA driver while handling a message (i.e. to read FAT data),
	// we'll overwrite the message data, as amc messages are always delivered to the same memory area
    char* source_service = strdup(msg->source);

	uint32_t event = amc_msg_u32_get_word(msg, 0);
	printf("File manager sent event %d from %s\n", event, source_service);
	if (event == FILE_MANAGER_READ_FILE) {
		file_manager_read_file_request_t* req = (file_manager_read_file_request_t*)&msg->body;
		fs_node_t* desired_file = _find_node_by_name(req->path);
		assert(desired_file, "Failed to find requested file");

		uint32_t file_size = 0;
		uint32_t response_size = 0;
		uint8_t* response_buffer = NULL;
		file_manager_read_file_response_t* resp = NULL;

		if (desired_file->base.type == FS_NODE_TYPE_INITRD) {
			initrd_fs_node_t* initrd_file = &desired_file->initrd;
			printf("Found file %s, initrd offset: 0x%08x\n", initrd_file->name, initrd_file->initrd_offset);

			file_size = initrd_file->size;
			response_size = sizeof(file_manager_read_file_response_t) + file_size;
			response_buffer = calloc(1, response_size);
			resp = (file_manager_read_file_response_t*)response_buffer;

			memcpy(&resp->file_data, initrd_file->initrd_offset, file_size);
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
				uint32_t fat_sector_index = fat_entry_index / fat_drive_info.fat_entries_per_sector;
				ata_sector_t* fat_sector = ata_read_sector(fat_drive_info.fat_head_sector + fat_sector_index);
				fat_entry_t* fat_sector_data = fat_sector->data;

				uint32_t next_fat_entry_index = fat_sector_data[fat_entry_index % fat_drive_info.fat_entries_per_sector].next_fat_entry_idx_in_file;
				printf("[FS] Followed link from FAT entry index %ld to %ld\n", fat_entry_index, next_fat_entry_index);

				// Read the actual sector
				uint32_t bytes_to_copy_from_sector = min(fat_drive_info.sector_size, file_bytes_remaining);
				file_bytes_remaining -= bytes_to_copy_from_sector;
				ata_sector_t* data_sector = ata_read_sector(fat_drive_info.fat_sector_slide + fat_entry_index);
				printf("copying %ld bytes from 0x%08x to 0x%08x, offset %ld\n", bytes_to_copy_from_sector, &data_sector->data, response_buffer + file_offset, file_offset);
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
		printf("Returning file size 0x%08x buf 0x%08x to %s, %s\n", resp->file_size, resp->file_data, source_service, response_buffer);
		if (desired_file->base.type == FS_NODE_TYPE_FAT) {
			hexdump(resp->file_data, resp->file_size);
		}
		amc_message_construct_and_send(source_service, resp, response_size);
		free(response_buffer);
	}
	else if (event == FILE_MANAGER_LAUNCH_FILE) {
		file_manager_launch_file_request_t* req = (file_manager_launch_file_request_t*)&msg->body;
		initrd_fs_node_t* desired_file = _find_node_by_name(req->path);
		if (desired_file) {
			printf("File Manager launching %s upon request\n", req->path);
			_launch_program_by_node(desired_file);
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

static image_t* _load_image(const char* name) {
	initrd_fs_node_t* fs_node = &(_find_node_by_name(name)->initrd);
	printf("_load_image got 0x%08x 0x%08x 0x%08x\n", fs_node, fs_node->size, fs_node->initrd_offset);
	assert(fs_node, "Failed to find an image at the provided path");
	return image_parse(fs_node->size, fs_node->initrd_offset);
}

void fat_format_drive(ata_drive_t drive, fat_drive_info_t* drive_info) {
	// Validate size assumptions
	assert(sizeof(fat_entry_t) == sizeof(uint32_t), "Expected a FAT entry to occupy exactly 4 bytes");
	assert(sizeof(fat_directory_entry_t) == 32, "Expected a FAT directory entry to occupy exactly 32 bytes");

	printf("[FAT] Formatting drive %d...\n", drive);

	// TODO(PT): Pull disk size & sector size from ATA driver. For now, assume 4MB
	drive_info->sector_size = 512;
	drive_info->disk_size_in_bytes = 4 * 1024 * 1024;
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
	fat_entry_t* fat_sector_data = fat_sector->data;
	fat_sector_data[0].allocated = true;
	fat_sector_data[0].next_fat_entry_idx_in_file = FAT_SECTOR_TYPE__EOF;
	ata_write_sector(drive_info->fat_head_sector, fat_sector_data);
	free(fat_sector);
	printf("[FAT] Finished formatting disk\n");
}

typedef struct fat_descriptor {
	//uint32_t fat_sector_lba;
	uint32_t fat_entry_idx;
	//uint32_t fat_entry_idx_within_sector;
	//uint32_t allocated_sector;
	uint32_t next_fat_entry_idx_in_file;
} fat_descriptor_t;

void fat_write_descriptor(fat_drive_info_t drive_info, fat_descriptor_t* desc) {
	uint32_t sector_index = desc->fat_entry_idx / drive_info.fat_entries_per_sector;
	uint32_t index_within_sector = desc->fat_entry_idx % drive_info.fat_entries_per_sector;
	ata_sector_t* fat_sector = ata_read_sector(sector_index);

	fat_entry_t* fat_sector_data = fat_sector->data;
	printf("[FAT] fat_write_descriptor [sector %ld] [idx %ld] [unslid %ld] [next %ld]\n", sector_index, index_within_sector, desc->fat_entry_idx, desc->next_fat_entry_idx_in_file);
	fat_sector_data[index_within_sector].next_fat_entry_idx_in_file = desc->next_fat_entry_idx_in_file;
	ata_write_sector(sector_index, fat_sector_data);

	free(fat_sector);
}

typedef struct fat_entry_descriptor {
	uint32_t fat_entry_idx;
	fat_entry_t ent;
} fat_entry_descriptor_t;

void fat_alloc_sector(fat_drive_info_t drive_info, uint32_t next_fat_entry_idx_in_file, fat_entry_descriptor_t* out_desc) {
	for (uint32_t i = 0; i < drive_info.fat_sector_count; i++) {
		uint32_t sector_index = drive_info.fat_head_sector + i;
		ata_sector_t* fat_sector = ata_read_sector(sector_index);
		fat_entry_t* fat_sector_data = fat_sector->data;

		for (uint32_t j = 0; j < drive_info.fat_entries_per_sector; j++) {
			if (fat_sector_data[j].allocated == false) {
				// Found a free FAT entry to allocate in
				uint32_t disk_sector = ((i * drive_info.fat_entries_per_sector) + j) + drive_info.fat_sector_slide;
				printf("[FAT] fat_alloc_sector allocating in FAT sector %ld (unslid %ld), index %ld (unslid %ld)\n", i, sector_index, j, disk_sector);
				fat_sector_data[j].allocated = true;
				fat_sector_data[j].next_fat_entry_idx_in_file = next_fat_entry_idx_in_file;
				
				// Write the FAT sector back to disk
				ata_write_sector(sector_index, fat_sector_data);
				free(fat_sector);

				// Retain a copy of the data we've written
				out_desc->fat_entry_idx = (i * drive_info.fat_entries_per_sector) + j;
				out_desc->ent.allocated = true;
				out_desc->ent.next_fat_entry_idx_in_file = next_fat_entry_idx_in_file;
				return;
			}
		}
		free(fat_sector);
	}
	assert(false, "FAT entirely full!");
}

uint32_t _fat_create_dir_or_file(fat_drive_info_t drive_info, uint32_t directory_fat_entry_index, bool is_directory, const char* filename, const char* ext, uint32_t file_len, const char* file_data) {
	printf("[FAT] _fat_create_dir_or_file (is dir? %ld, directory fat entry idx %ld) %s.%s %ld\n", is_directory, directory_fat_entry_index, filename, ext, file_len);

	// Find the disk sector containing the directory data
	uint32_t directory_sector_index = directory_fat_entry_index + drive_info.fat_sector_slide;
	printf("\tDirectory stored in sector %ld\n", directory_sector_index);
	/*
	uint32_t fat_sector_index = directory_fat_entry_index / fat_drive_info.fat_entries_per_sector;
	ata_sector_t* fat_sector = ata_read_sector(fat_drive_info.fat_head_sector + fat_sector_index);
	fat_entry_t* fat_sector_data = fat_sector->data;
	uint32_t directory_sector_idx = fat_sector_data[directory_fat_entry_index % drive_info.fat_entries_per_sector];
	free(fat_sector);
	*/

	ata_sector_t* directory_sector = ata_read_sector(directory_sector_index);
	fat_directory_t directory = {
		.slot_count = drive_info.sector_size / sizeof(fat_directory_entry_t),
		.slots = directory_sector->data
	};

	// Iterate the slots in the directory until we find a free one
	for (uint32_t i = 0; i < directory.slot_count; i++) {
		printf("\tDirectory slot %ld: first entry %ld\n", i, directory.slots[i].first_fat_entry_idx_in_file);
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
			uint32_t remaining_byte_count_to_write = file_len;

			// Only the last sector may have less data to write than a full sector
			uint32_t bytes_to_write_in_next_sector = file_bytes_in_terminating_sector;
			uint8_t* file_ptr = (file_data + file_len) - bytes_to_write_in_next_sector;

			for (uint32_t j = 0; j < needed_sectors; j++) {
				fat_entry_descriptor_t out_desc = {0};
				fat_alloc_sector(fat_drive_info, next_fat_entry_idx, &out_desc);
				next_fat_entry_idx = out_desc.fat_entry_idx;
				printf("\tAllocated fat entry %ld, next in file: %ld\n", out_desc.fat_entry_idx, out_desc.ent.next_fat_entry_idx_in_file);

				// Write the corresponding file data in the data sector associated with the FAT entry
				uint32_t disk_sector = out_desc.fat_entry_idx + drive_info.fat_sector_slide;
				printf("\tWriting %ld of file data to sector %ld...\n", bytes_to_write_in_next_sector, disk_sector);

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
	uint32_t first_fat_entry_idx = _fat_create_dir_or_file(fat_drive_info, parent_directory->first_fat_entry_idx_in_file, true, filename, NULL, 0, NULL);

	char fat_filename[32];
	snprintf(fat_filename, sizeof(fat_filename), "%.*s", FAT_FILENAME_SIZE, filename);

	fat_fs_node_t* new_node = (fat_fs_node_t*)fs_node_create__directory(parent_directory, fat_filename, strlen(fat_filename));
	new_node->base.type = FS_NODE_TYPE_FAT;
	new_node->first_fat_entry_idx_in_file = first_fat_entry_idx;
	// TODO(PT): Change me when directories can be larger
	new_node->size = fat_drive_info.sector_size;
	return new_node;
}

fat_fs_node_t* fat_create_file(fat_fs_node_t* parent_directory, const char* filename, const char* ext, uint32_t file_len, const char* file_data) {
	assert(parent_directory->base.type == FS_NODE_TYPE_FAT, "Can only create a FAT file within a FAT directory");
	uint32_t first_fat_entry_idx = _fat_create_dir_or_file(fat_drive_info, parent_directory->first_fat_entry_idx_in_file, false, filename, ext, file_len, file_data);

	char fat_filename[32];
	snprintf(fat_filename, sizeof(fat_filename), "%.*s.%.*s", sizeof(FAT_FILENAME_SIZE), filename, sizeof(FAT_FILE_EXT_SIZE), ext);

	fat_fs_node_t* new_node = (fat_fs_node_t*)fs_node_create__file(parent_directory, fat_filename, strlen(fat_filename));
	new_node->base.type = FS_NODE_TYPE_FAT;
	new_node->first_fat_entry_idx_in_file = first_fat_entry_idx;
	new_node->size = file_len;
	return new_node;
}

void _key_down(gui_elem_t* gui_elem, uint32_t ch) {
	printf("KEY DOWN\n");
	fat_fs_node_t* root = _find_node_by_name("/hdd/heyyy");
	printf("Root: %s\n", root->base.name);
	static int i = 0; 

	uint32_t t = ms_since_boot() / 100;
	char name[8];
	snprintf(name, 8, "%ld", t);
	char* data = calloc(1, 256);
	memset(data, 'A' + i, 256);
	fat_create_file(root, name, "txt", 256, data);
	free(data);
	i += 1;

	_print_fs_tree((fs_node_t*)_find_node_by_name("/hdd"), 0);
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
	printf("0x%08x 0x%08x (0x%08x bytes)\n", initrd_info->initrd_start, initrd_info->initrd_end, initrd_info->initrd_size);

	char* root_path = "/";
	fs_base_node_t* root = fs_node_create__directory(NULL, root_path, strlen(root_path));
	root_fs_node = root;
	root->type = FS_NODE_TYPE_ROOT;

	char* initrd_path = "initrd";
	fs_base_node_t* initrd_root = fs_node_create__directory(root, initrd_path, strlen(initrd_path));
	_parse_initrd(initrd_root, initrd_info);

	// TODO(PT): If we need to format the hard drive, do it now
	// Otherwise, fat_drive_info should be populated by reading the necessary info from disk
	if (true) {
		fat_format_drive(ATA_DRIVE_MASTER, &fat_drive_info);
	}
	else {
		// TODO(PT): Fragile magic numbers
		fat_drive_info.sector_size = 512;
		fat_drive_info.disk_size_in_bytes = 4 * 1024 * 1024;

		fat_drive_info.fat_sector_count = 64;
		fat_drive_info.fat_head_sector = 1;
		fat_drive_info.root_directory_head_sector = 65;
		fat_drive_info.fat_sector_slide = 65;

		fat_drive_info.sectors_on_disk = 8192;
		fat_drive_info.fat_entries_per_sector = 16;
	}

	fat_fs_node_t* fat_root = _parse_fat(root, fat_drive_info);

	/*
	fat_fs_node_t* dir = fat_create_directory(fat_root, "heyyy");
	char* data = calloc(1, 256);
	memset(data, 'B', 100);
	fat_create_file(dir, "inner", "abc", 100, data);
	free(data);
	*/

	_print_fs_tree((fs_node_t*)root, 0);

	_g_folder_icon = _load_image("/initrd/folder_icon.bmp");
	_g_image_icon = _load_image("/initrd/image_icon.bmp");
	_g_executable_icon = _load_image("/initrd/executable_icon.bmp");
	_g_text_icon = _load_image("/initrd/text_icon.bmp");

	gui_window_t* window = gui_window_create("File Manager", 400, 600);
	gui_scroll_view_t* content_view = gui_scroll_view_create(
		window, 
		(gui_window_resized_cb_t)_content_view_sizer
	);
	content_view->base.background_color = color_white();
	// TODO(PT): Traverse the tree beforehand to build the depth listing, 
	// then generate the UI tree without having to traverse the node tree each time
	_generate_ui_tree((gui_view_t*)content_view, NULL, 0, (fs_node_t*)root);

	gui_add_message_handler(_amc_message_received);
	amc_msg_u32_1__send(AWM_SERVICE_NAME, FILE_MANAGER_READY);
	content_view->base.key_down_cb = _key_down;

	gui_enter_event_loop();

	return 0;
}
