#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdint.h>
#include "html.h"
#include "css.h"
#include "shims.h"

typedef enum layout_mode {
    ROOT_LAYOUT = 0,
    BLOCK_LAYOUT = 1,
    INLINE_LAYOUT = 2,
    TEXT_LAYOUT = 3
} layout_mode_t;

typedef struct layout_node_base {
    // Shared union members
    layout_mode_t mode;
    bool is_anonymous;
    bool is_block_formatting_context;
    bool is_inline_formatting_context;
    html_dom_node_t* dom_node;

	struct layout_node_base* parent;
    uint32_t idx_within_parent;

	struct layout_node_base** children;
	uint32_t max_children;
	uint32_t child_count;

    Rect margin_frame;
    Rect content_frame;

    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_left;
    uint32_t margin_right;

    Size font_size;
    Color font_color;

    bool sets_background_color;
    Color background_color;
} layout_node_base_t;

typedef struct layout_block_node {
    // Shared union members
    layout_mode_t mode;
    bool is_anonymous;
    bool is_block_formatting_context;
    bool is_inline_formatting_context;
    html_dom_node_t* dom_node;

	struct layout_node_base* parent;
    uint32_t idx_within_parent;

	struct layout_node_base** children;
	uint32_t max_children;
	uint32_t child_count;

    Rect margin_frame;
    Rect content_frame;

    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_left;
    uint32_t margin_right;

    Size font_size;
    Color font_color;

    bool sets_background_color;
    Color background_color;

    // Public members
    array_t* line_boxes;
} layout_block_node_t;

typedef struct layout_inline_node {
    // Shared union members
    layout_mode_t mode;
    bool is_anonymous;
    bool is_block_formatting_context;
    bool is_inline_formatting_context;
    html_dom_node_t* dom_node;

	struct layout_node_base* parent;
    uint32_t idx_within_parent;

	struct layout_node_base** children;
	uint32_t max_children;
	uint32_t child_count;

    Rect margin_frame;
    Rect content_frame;

    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_left;
    uint32_t margin_right;

    Size font_size;
    Color font_color;

    bool sets_background_color;
    Color background_color;

    // Public members
} layout_inline_node_t;

typedef struct layout_text_node {
    // Shared union members
    layout_mode_t mode;
    bool is_anonymous;
    bool is_block_formatting_context;
    bool is_inline_formatting_context;
    html_dom_node_t* dom_node;

	struct layout_node_base* parent;
    uint32_t idx_within_parent;

	struct layout_node_base** children;
	uint32_t max_children;
	uint32_t child_count;

    Rect margin_frame;
    Rect content_frame;

    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_left;
    uint32_t margin_right;

    Size font_size;
    Color font_color;

    bool sets_background_color;
    Color background_color;

    // Public members
    char* text;
} layout_text_node_t;

typedef struct layout_root_node {
    // Shared union members
    layout_mode_t mode;
    bool is_anonymous;
    bool is_block_formatting_context;
    bool is_inline_formatting_context;
    html_dom_node_t* dom_node;

	struct layout_node_base* parent;
    uint32_t idx_within_parent;

	struct layout_node_base** children;
	uint32_t max_children;
	uint32_t child_count;

    Rect margin_frame;
    Rect content_frame;

    uint32_t margin_top;
    uint32_t margin_bottom;
    uint32_t margin_left;
    uint32_t margin_right;

    Size font_size;
    Color font_color;

    bool sets_background_color;
    Color background_color;
} layout_root_node_t;

typedef struct line_fragment {
    uint32_t start_idx;
    uint32_t length;
    layout_text_node_t* node;
    Rect frame;
} line_fragment_t;

typedef struct line_box {
    Rect frame;
    uint32_t max_width;
    array_t* fragments;
} line_box_t;

typedef union layout_node {
    layout_node_base_t base_node;
    layout_root_node_t root_node;
    layout_block_node_t block_node;
    layout_inline_node_t inline_node;
    layout_text_node_t text_node;
} layout_node_t;

layout_root_node_t* layout_generate(html_dom_node_t* root, array_t* css_nodes, uint32_t window_width);

#endif