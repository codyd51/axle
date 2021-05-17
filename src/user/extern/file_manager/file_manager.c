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
#include <libimg/libimg.h>

#include "vfs.h"
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
	image_bmp_t* icon;
} file_view_t;

static fs_node_t* root_fs_node = NULL;
static image_bmp_t* _g_folder_icon = NULL;
static image_bmp_t* _g_image_icon = NULL;
static image_bmp_t* _g_executable_icon = NULL;
static image_bmp_t* _g_text_icon = NULL;

static initrd_fs_node_t* _find_node_by_name(char* name);
bool str_ends_with(char* str, char* suffix);

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
		fs_node->initrd_offset = file_header->offset;
		fs_node->size = file_header->length;

		offset += sizeof(initrd_file_header_t);
	}
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
	else if (node->base.type == FS_NODE_TYPE_ROOT) {
		printf(" (Root)>\n");
	}
	else if (node->base.type == FS_NODE_TYPE_BASE) {
		printf(">\n");
	}
	else {
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
	if (str_ends_with(file_name, ".bmp")) {
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
	else if (str_ends_with(node->base.name, ".bmp")) {
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

static initrd_fs_node_t* _find_node_by_name(char* name) {
	fs_base_node_t* initrd = array_lookup(root_fs_node->base.children, 0);
	for (uint32_t i = 0; i < initrd->children->size; i++) {
		initrd_fs_node_t* child = array_lookup(initrd->children, i);
		if (!strcmp(child->name, name)) {
			return child;
		}
	}
	return NULL;
}

static void _amc_message_received(gui_window_t* window, amc_message_t* msg) {
    const char* source_service = msg->source;

	uint32_t event = amc_msg_u32_get_word(msg, 0);
	printf("File manager sent event %d\n", event);
	if (event == FILE_MANAGER_READ_FILE) {
		file_manager_read_file_request_t* req = (file_manager_read_file_request_t*)&msg->body;
		initrd_fs_node_t* desired_file = _find_node_by_name(req->path);
		assert(desired_file, "Failed to find requested file");

		uint32_t response_size = sizeof(file_manager_read_file_response_t) + desired_file->size;
		uint8_t* response_buffer = calloc(1, response_size);

		file_manager_read_file_response_t* resp = (file_manager_read_file_response_t*)response_buffer;
		resp->event = FILE_MANAGER_READ_FILE_RESPONSE;
		resp->file_size = desired_file->size;
		memcpy(&resp->file_data, desired_file->initrd_offset, resp->file_size);
		printf("Returning file size 0x%08x buf 0x%08x\n", resp->file_size, resp->file_data);
		amc_message_construct_and_send(source_service, resp, response_size);
		free(response_buffer);
	}
	else {
		assert(false, "Unknown message sent to file manager");
	}
}

static image_bmp_t* _load_image(const char* name) {
	initrd_fs_node_t* fs_node = _find_node_by_name(name);
	return image_parse_bmp(fs_node->size, fs_node->initrd_offset);
}

int main(int argc, char** argv) {
	amc_register_service(FILE_MANAGER_SERVICE_NAME);

	gui_window_t* window = gui_window_create("File Manager", 400, 600);
	Size window_size = window->size;

	gui_scroll_view_t* content_view = gui_scroll_view_create(window, (gui_window_resized_cb_t)_content_view_sizer);
	content_view->background_color = color_white();

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

	_g_folder_icon = _load_image("folder_icon.bmp");
	_g_image_icon = _load_image("image_icon.bmp");
	_g_executable_icon = _load_image("executable_icon.bmp");
	_g_text_icon = _load_image("text_icon.bmp");

	gui_add_message_handler(window, _amc_message_received);

	_print_fs_tree((fs_node_t*)root, 0);
	_generate_ui_tree((gui_view_t*)content_view, NULL, 0, (fs_node_t*)root);

	gui_enter_event_loop(window);

	return 0;
}
