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

#include "vfs.h"

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
    ca_layer* content_layer;
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
	struct gui_view superview;
    uint32_t border_margin;
    uint32_t title_bar_height;
    char* _title;
    Rect _title_inset;

	// File-view-specific fields
	fs_node_t* fs_node;
	Point parent_folder_start;
	uint32_t idx_within_folder;
	uint32_t dfs_index;
} file_view_t;

static void _draw_string(ca_layer* layer, char* text, Point origin, Size font_size, Color color) {
	Point cursor = origin;
	for (uint32_t i = 0; i < strlen(text); i++) {
		draw_char(
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

static void _draw_centered_string(ca_layer* layer, char* text, Point center, Size font_size, Color color) {
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
	printf("nfiles %d\n", header->nfiles);
	for (uint32_t i = 0; i < header->nfiles; i++) {
		initrd_file_header_t* file_header = (initrd_file_header_t*)offset;

		assert(file_header->magic == HEADER_MAGIC, "Initrd file header magic was wrong");

		initrd_fs_node_t* fs_node = (initrd_fs_node_t*)fs_node_create__file(initrd_root, file_header->name, strlen(file_header->name));
		printf("created node %s\n", file_header->name);
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
		printf("(node %s find %s) found outer\n", parent->name, find->name);
		*out_found = true;
		return sum;
	}

	if (parent->children) {
		printf("(node %s find %s) children\n", parent->name, find->name);
		for (uint32_t i = 0; i < parent->children->size; i++) {
			fs_base_node_t* child = array_lookup(parent->children, i);
			sum = _depth_first_search__idx(child, find, sum, out_found);

			if (*out_found) {
				return sum;
			}
		}
	}
	printf("(node %s find %s) return sum %d\n", parent->name, find->name, sum);
	return sum;
}

static Rect _file_view_sizer(file_view_t* view, Size window_size) {
	Size icon_size = size_make(30, 30);
	uint32_t padding_y = 10;

	bool found = false;

	if (view->fs_node->base.type != FS_NODE_TYPE_ROOT) {
		fs_node_t* root = view->fs_node->base.parent;
		while (root->base.type != FS_NODE_TYPE_ROOT) {
			root = root->base.parent;
		}
		view->dfs_index = _depth_first_search__idx(root, view->fs_node, 0, &found);
		printf("Depth of %s: %d\n", view->fs_node->base.name, view->dfs_index);
	}

	Point origin = view->parent_folder_start;
	if (view->fs_node->base.type == FS_NODE_TYPE_ROOT) {
		printf("superview 0x%08x %d %d\n", view->superview, view->superview.content_layer_frame.origin.x, view->superview.content_layer_frame.origin.y);
		origin = point_make(24, 24);
		//origin = view->superview.content_layer_frame.origin;
	}
	else {
		// Indent from the parent
		origin.x += icon_size.width;
		// Indent from previous siblings within this directory
		origin.y = (view->dfs_index) * (icon_size.height + padding_y);
		//origin.y += ((view->idx_within_folder + 1) * (icon_size.height + padding_y));
	}

	return rect_make(
		origin,
		icon_size
	);
}

static void _file_view_draw(file_view_t* view, bool is_active) {
	Color bg_color = is_active ? color_blue() : color_black();
	Size font_size = size_make(8, 12);
	Rect icon_frame = rect_make(
		view->frame.origin,
		size_make(
			view->frame.size.width, 
			view->frame.size.height /*- (font_size.height * 1)*/
		)
	);
	if (rect_min_y(icon_frame) >= view->window->layer->size.height) {
		return;
	}
	/*
	Rect label_frame = rect_make(
		point_make(
			view->frame.origin.x,
			rect_max_y(icon_frame)
		),
		size_make(
			view->frame.size.width,
			view->frame.size.height - icon_frame.size.height
		)
	);
	*/
	draw_rect(
		view->window->layer, 
		icon_frame,
		color_light_gray(),
		THICKNESS_FILLED
	);
	_draw_centered_string(
		view->window->layer, 
		"?",
		point_make(
			rect_mid_x(icon_frame),
			rect_mid_y(icon_frame)
		),
		font_size,
		color_white()
	);
	draw_rect(
		view->window->layer, 
		icon_frame,
		bg_color,
		1
	);

	Rect label_frame = rect_make(
		point_make(
			rect_max_x(icon_frame),
			rect_min_y(icon_frame)
		),
		icon_frame.size
	);
	_draw_string(
		view->window->layer, 
		view->fs_node->base.name,
		point_make(
			rect_min_x(label_frame) + font_size.width,
			rect_mid_y(label_frame) - (font_size.height / 2.0)
		),
		font_size,
		color_black()
	);

	// Connecting line to the tree structure
	// Horizontal line
	draw_line(
		view->window->layer,
		line_make(
			point_make(
				view->parent_folder_start.x + (view->frame.size.width / 2),
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
	//if (view->idx_within_folder == 3) {
		draw_line(
			view->window->layer,
			line_make(
				point_make(
					view->parent_folder_start.x + (view->frame.size.width / 2.0),
					view->parent_folder_start.y + (view->frame.size.height)
				),
				point_make(
					view->parent_folder_start.x + (view->frame.size.width / 2.0),
					rect_mid_y(icon_frame)
				)
			),
			color_light_gray(),
			1
		);
	//}
}

static void _file_view_left_click(file_view_t* view, Point mouse_point) {
	assert(view->fs_node->base.type == FS_NODE_TYPE_INITRD, "Can only launch initrd programs");

	amc_exec_buffer_cmd_t cmd = {0};
	cmd.event = AMC_FILE_MANAGER_EXEC_BUFFER;
	cmd.program_name = view->fs_node->initrd.name;
	cmd.buffer_addr = (void*)view->fs_node->initrd.initrd_offset;
	cmd.buffer_size = view->fs_node->initrd.size;
	amc_message_construct_and_send(AXLE_CORE_SERVICE_NAME, &cmd, sizeof(amc_exec_buffer_cmd_t));
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
	gui_view_add_subview(container_view, (gui_view_t*)file_view);
	file_view->_priv_draw_cb = (gui_draw_cb_t)_file_view_draw;
	file_view->left_click_cb = (gui_mouse_left_click_cb_t)_file_view_left_click;

	if (node->base.children) {
		for (uint32_t i = 0; i < node->base.children->size; i++) {
			fs_node_t* child = array_lookup(node->base.children, i);
			_generate_ui_tree(container_view, file_view, i, child);
		}
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.file_manager");

	gui_window_t* window = gui_window_create("File Manager", 300, 860);
	Size window_size = window->size;

	gui_view_t* content_view = gui_view_create(window, (gui_window_resized_cb_t)_content_view_sizer);
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
	root->type = FS_NODE_TYPE_ROOT;

	char* initrd_path = "initrd";
	fs_base_node_t* initrd_root = fs_node_create__directory(root, initrd_path, strlen(initrd_path));
	_parse_initrd(initrd_root, initrd_info);

	_print_fs_tree((fs_node_t*)root, 0);
	_generate_ui_tree(content_view, NULL, 0, (fs_node_t*)root);

	gui_enter_event_loop(window);

	return 0;
}
