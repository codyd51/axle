#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stdlibadd/assert.h>
#include <agx/font/font.h>
#include <libamc/libamc.h>
#include <libimg/libimg.h>
#include <libgui/libgui.h>
#include <image_viewer/image_viewer_messages.h>

#include "initrd.h"
#include "util.h"
#include "vfs.h"
#include "ui.h"

static const char* _g_image_extensions[] = {".bmp", ".jpg", ".jpeg", NULL};

static image_t* _g_folder_icon = NULL;
static image_t* _g_image_icon = NULL;
static image_t* _g_executable_icon = NULL;
static image_t* _g_text_icon = NULL;

static image_t* _load_image(const char* name) {
	initrd_fs_node_t* fs_node = &(vfs_find_node_by_name(name)->initrd);
	printf("_load_image got 0x%08x 0x%08x 0x%08x\n", fs_node, fs_node->size, fs_node->initrd_offset);
	assert(fs_node, "Failed to find an image at the provided path");
	return image_parse(fs_node->size, (uint8_t*)fs_node->initrd_offset);
}

void file_manager_load_images(void) {
	_g_folder_icon = _load_image("/initrd/folder_icon.bmp");
	_g_image_icon = _load_image("/initrd/image_icon.bmp");
	_g_executable_icon = _load_image("/initrd/executable_icon.bmp");
	_g_text_icon = _load_image("/initrd/text_icon.bmp");
}

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

static file_view_t* _file_view_alloc(const char* filename) {
	file_view_t* f = calloc(1, sizeof(file_view_t));
	gui_view_alloc_dynamic_fields((gui_view_t*)f);
	return f;
}

Rect ui_content_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

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
		launch_amc_service_if_necessary(IMAGE_VIEWER_SERVICE_NAME);

		image_viewer_load_image_request_t req = {0};
		req.event = IMAGE_VIEWER_LOAD_IMAGE;
		snprintf(req.path, sizeof(req.path), "%s", file_name);
		amc_message_construct_and_send(IMAGE_VIEWER_SERVICE_NAME, &req, sizeof(image_viewer_load_image_request_t));
	}
	else if (str_ends_with(file_name, ".txt")) {
		printf("Ignoring click on text file until text viewer is available: %s\n", file_name);
	}
	else {
		vfs_launch_program_by_node(view->fs_node);
	}
}

void ui_generate_tree(gui_view_t* container_view, file_view_t* parent_view, uint32_t idx_within_parent, fs_node_t* node) {
	//const char* type = node->base.is_directory ? "Dir" : "File";

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
		file_view->dfs_index = depth_first_search__idx((fs_base_node_t*)root, (fs_base_node_t*)node, 0, &found);
		printf("Depth of %s: %ld\n", node->base.name, file_view->dfs_index);
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
			ui_generate_tree(container_view, file_view, i, child);
		}
	}
}
