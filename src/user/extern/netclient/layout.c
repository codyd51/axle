#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>
#include <stdio.h>

#include "shims.h"
#include "layout.h"
#include "utils.h"

// Take HTML DOM
// Spit out a tree of layout commands

// TODO(PT): Quick hack, should be removed and replaced with a CSSOM + render tree
#define FONT_HEIGHT 20

#define HORIZONTAL_STEP 10
#define VERTICAL_STEP 8

static void _layout_node_add_child(layout_node_base_t* parent, layout_node_t* child);

static html_dom_node_t* _html_child_tag_with_name(html_dom_node_t* parent_node, char* tag_name) {
	for (uint32_t i = 0; i < parent_node->child_count; i++) {
		html_dom_node_t* child_node = parent_node->children[i];
		if (child_node->type == HTML_DOM_NODE_TYPE_HTML_TAG) {
			// https://stackoverflow.com/questions/2661766/how-do-i-lowercase-a-string-in-c
			char* lower = strdup(child_node->name);
			char* p = lower;
			for ( ; *p; ++p) *p = tolower(*p);

			if (!strcmp(lower, tag_name)) {
				free(lower);
				return child_node;
			}
			free(lower);
		}
	}
	return NULL;
}

static layout_node_t* _layout_node_alloc(void) {
	layout_node_t* node = calloc(1, sizeof(layout_node_t));

	node->base_node.child_count = 0;
	node->base_node.max_children = 32;
	node->base_node.children = calloc(node->base_node.max_children, sizeof(layout_node_t));
	return node;
}

static void _layout_node_apply_css_styling(layout_node_base_t* node, array_t* css_nodes) {
	// Inherit parent attributes
	if (node->parent) {
		layout_node_base_t* parent = node->parent;
		if (parent->sets_background_color) {
			node->sets_background_color = true;
			node->background_color = parent->background_color;
		}
		node->font_size = parent->font_size;
	}

	// Apply CSS styling
	for (uint32_t i = 0; i < css_nodes->size; i++) {
		css_node_t* css_node = array_lookup(css_nodes, i);

		// The root node does not have a DOM node, we'll need another way to match it to a CSS node
		if (node->mode == ROOT_LAYOUT) {
			continue;
		}

		if (!strncmp(node->dom_node->name, css_node->name, 64)) {
			printf("Applying styling of CSS node <%s>\n", css_node->name);
			if (css_node->sets_background_color) {
				node->sets_background_color = true;
				node->background_color = css_node->background_color;
			}
			if (css_node->sets_font_size_em) {
				uint32_t em_scale = css_node->font_size_em;
				node->font_size = size_make(8 * em_scale, 12 * em_scale);
			}
			if (css_node->sets_margin_top) {
				node->margin_top = css_node->margin_top;
			}
			if (css_node->sets_margin_bottom) {
				node->margin_bottom = css_node->margin_bottom;
			}
			if (css_node->sets_margin_left) {
				node->margin_left = css_node->margin_left;
			}
			if (css_node->sets_margin_right) {
				node->margin_right = css_node->margin_right;
			}
			break;
		}
	}

	// Our height is the font height
	// TODO(PT): Create another layout node if we need to spill onto the next line
	if (node->mode == INLINE_LAYOUT && node->dom_node && node->dom_node->type == HTML_DOM_NODE_TYPE_TEXT) {
		layout_inline_node_t* inline_node = (layout_inline_node_t*)node;
		uint32_t font_height = inline_node->font_size.height;
		inline_node->content_frame.size.height = font_height;
		inline_node->margin_frame.size.height = font_height + inline_node->margin_top + inline_node->margin_bottom;
		inline_node->text = strdup(inline_node->dom_node->name);
	}
}

static layout_root_node_t* layout_node_create__root_node(uint32_t window_width, array_t* css_nodes) {
	layout_root_node_t* node = &_layout_node_alloc()->root_node;
    node->mode = ROOT_LAYOUT;

	node->margin_top = VERTICAL_STEP;
	node->margin_bottom = VERTICAL_STEP;
	node->margin_left = HORIZONTAL_STEP;
	node->margin_right = HORIZONTAL_STEP;
	node->font_size = size_make(8, 12);

	node->margin_frame = rect_make(point_zero(), size_make(window_width, 0));
	node->content_frame = rect_make(
		point_make(
			node->margin_left,
			node->margin_top
		), 
		size_make(
			node->margin_frame.size.width - node->margin_left - node->margin_right, 
			0
		)
	);

	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);

	return node;
}

static layout_block_node_t* layout_node_create__block_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_block_node_t* node = &_layout_node_alloc()->block_node;
    node->mode = BLOCK_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = dom_node;
	_layout_node_add_child(parent, (layout_node_t*)node);

	node->margin_top = 0;
	node->margin_bottom = 0;
	node->margin_left = 0;
	node->margin_right = 0;
	//node->font_size = parent->base_node.font_size;

	/*
	if (!strcmp(node->dom_node->name, "h1")) {
		printf("*** Found h1\n");
		node->margin_top = FONT_HEIGHT * 0.67;
		node->margin_bottom = FONT_HEIGHT * 0.67;
		node->font_size = size_make(12, 24);
	}
	*/

	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);

	return node;
}

static layout_inline_node_t* layout_node_create__inline_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_inline_node_t* node = &_layout_node_alloc()->inline_node;
    node->mode = INLINE_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = dom_node;
	_layout_node_add_child(parent, (layout_node_t*)node);

	node->margin_top = 0;
	node->margin_bottom = 0;
	node->margin_left = 0;
	node->margin_right = 0;
	//node->font_size = parent->base_node.font_size;
	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);

	return node;
}

static void _layout_node_add_child(layout_node_base_t* parent, layout_node_t* child) {
	if (parent->child_count + 1 >= parent->max_children) {
		uint32_t new_max = parent->max_children * 2;
		printf("Resizing node's children from %d -> %d\n", parent->max_children, new_max);
		parent->children = realloc(parent->children, sizeof(parent->children[0]) * new_max);
		parent->max_children = new_max;
	}
	child->base_node.idx_within_parent = parent->child_count;
	parent->child_count += 1;
	parent->children[child->base_node.idx_within_parent] = &child->base_node;
	child->base_node.parent = parent;
}

static layout_node_t* _layout_node_prev_sibling(layout_node_t* n) {
	// Root node?
	if (!n->base_node.parent) {
		return NULL;
	}
	// First child (ie no previous sibling)?
	if (n->base_node.idx_within_parent == 0) {
		return NULL;
	}
	return (layout_node_t*)n->base_node.parent->children[n->base_node.idx_within_parent - 1];
}

static layout_mode_t _layout_mode_for_dom_node(html_dom_node_t* dom_node) {
	// There's no reason this must be the case, but we currently create the root layout node separately
	assert(dom_node->type != HTML_DOM_NODE_TYPE_DOCUMENT, "Document layout node is created separately");

	// Text is always laid out inline
	if (dom_node->type == HTML_DOM_NODE_TYPE_TEXT) {
		return INLINE_LAYOUT;
	}

	// Is it a block tag?
	// https://browser.engineering/layout.html
	const char* block_tags[] = {
		"html", "body", "article", "section", "nav", "aside",
		"h1", "h2", "h3", "h4", "h5", "h6", "hgroup", "header",
		"footer", "address", "p", "hr", "ol", "ul", "menu", "li",
		"dl", "dt", "dd", "figure", "figcaption", "main", "div",
		"table", "form", "fieldset", "legend", "details", "summary"
	};
	for (uint32_t i = 0; i < sizeof(block_tags) / sizeof(block_tags[0]); i++) {
		const char* block_tag = block_tags[i];

		if (!strcicmp(dom_node->name, block_tag)) {
			return BLOCK_LAYOUT;
		}
	}
	return INLINE_LAYOUT;
}

static layout_node_t* _layout_node_from_dom_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes);
static layout_node_t* _layout_block_node_from_dom_node__block(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes);
static layout_node_t* _layout_inline_node_from_dom_node__inline(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes);

static layout_node_t* _layout_node_from_dom_node__block(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_block_node_t* block = layout_node_create__block_node(parent, dom_node, css_nodes);

	// Block nodes take up the parent's width
	uint32_t full_width = parent->content_frame.size.width;
	block->margin_frame.size.width = full_width;
	// Subtract the left and right margins of the block from its content width
	uint32_t content_width = full_width - block->margin_left - block->margin_right;
	block->content_frame.size.width = content_width;

	// And starts at its parents left content edge
	block->margin_frame.origin.x = rect_min_x(parent->content_frame);
	// And include the left margin in our content frame
	block->content_frame.origin.x = block->margin_frame.origin.x + block->margin_left;

	// Vertically, we start at the top of the container or just below our previous sibling
	layout_node_base_t* previous_sibling = (layout_node_base_t*)_layout_node_prev_sibling((layout_node_t*)block);
	if (previous_sibling) {
		// Start below the previous sibling
		uint32_t prev_max_y = rect_max_y(previous_sibling->margin_frame);
		block->margin_frame.origin.y = prev_max_y;

		/*
		// And add the previous sibling's bottom margin
		//printf("Previous sibling %s mode %d bottom margin %d\n", previous_sibling->base_node.dom_node->name, previous_sibling->base_node.mode, previous_sibling->block_node.margin_bottom);
		if (previous_sibling->mode == BLOCK_LAYOUT) {
			printf("add bottom margin\n");
			block->margin_frame.origin.y += previous_sibling->margin_bottom;
		}
		*/
	}
	else {
		block->margin_frame.origin.y = rect_min_y(parent->content_frame);
	}
	// And add in the top margin
	block->content_frame.origin.y = block->margin_frame.origin.y + block->margin_top;

    for (uint32_t i = 0; i < dom_node->child_count; i++) {
        html_dom_node_t* dom_child = dom_node->children[i];
		layout_node_t* child = _layout_node_from_dom_node((layout_node_base_t*)block, dom_child, css_nodes);
    }

	// Now set the height of a block
	// A block should be tall enough to contain all its children
	uint32_t height_sum = 0;
	for (uint32_t i = 0; i < block->child_count; i++) {
		layout_node_base_t* child = block->children[i];
		height_sum += child->margin_frame.size.height;
	}
	block->content_frame.size.height = height_sum;
	// And add in the top and bottom margin to the full frame
	block->margin_frame.size.height = block->content_frame.size.height + block->margin_top + block->margin_bottom;

	return (layout_node_t*)block;
}

static layout_node_t* _layout_node_from_dom_node__inline(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_inline_node_t* inline_box = layout_node_create__inline_node(parent, dom_node, css_nodes);

	// Inline nodes take up the parent's width
	uint32_t full_width = parent->content_frame.size.width;
	inline_box->margin_frame.size.width = full_width;
	// Subtract the left and right margins of the block from its content width
	uint32_t content_width = full_width - inline_box->margin_left - inline_box->margin_right;
	inline_box->content_frame.size.width = content_width;

	// And starts at its parents left content edge
	inline_box->margin_frame.origin.x = rect_min_x(parent->content_frame);
	// And include the left margin in our content frame
	inline_box->content_frame.origin.x = inline_box->margin_frame.origin.x + inline_box->margin_left;

	// Vertically, we start at the top of the container or just below our previous sibling
	layout_node_base_t* previous_sibling = (layout_node_base_t*)_layout_node_prev_sibling((layout_node_t*)inline_box);
	if (previous_sibling) {
		// Start below the previous sibling
		uint32_t prev_max_y = rect_max_y(previous_sibling->margin_frame);
		inline_box->margin_frame.origin.y = prev_max_y;
	}
	else {
		inline_box->margin_frame.origin.y = rect_min_y(parent->content_frame);
	}
	// And add in the top margin
	inline_box->content_frame.origin.y = inline_box->margin_frame.origin.y + inline_box->margin_top;

	// Inline nodes have no children (for now?)
	if (dom_node->child_count > 0) {
		printf("Inline node with children: <%s %s>\n", dom_node->name, dom_node->attrs);
	}
	assert(dom_node->child_count == 0, "Children of inline nodes are not supported");

	return (layout_node_t*)inline_box;
}

static layout_node_t* _layout_node_from_dom_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_mode_t mode = _layout_mode_for_dom_node(dom_node);

	layout_node_t* node = NULL;
	switch (mode) {
		case BLOCK_LAYOUT:
			node = (layout_node_t*)_layout_node_from_dom_node__block(parent, dom_node, css_nodes);
			break;
		case INLINE_LAYOUT:
			node = (layout_node_t*)_layout_node_from_dom_node__inline(parent, dom_node, css_nodes);
			break;
		default:
			assert(false, "Unknown layout type");
			break;
	}
	
	return node;
}

void _create_layout_node(html_dom_node_t* node) {
    for (uint32_t i = 0; i < node->child_count; i++) {
        html_dom_node_t* child = node->children[i];
    }
}

static void _print_tabs(uint32_t count) {
	if (count > 0) {
		putchar('\t');
		for (uint32_t i = 0; i < count-1; i++) {
			putchar('\t');
		}
	}
}

static void _print_layout_node(layout_node_t* node, uint32_t depth) {
	const char* node_type = "Unknown";
	switch (node->base_node.mode) {
		case ROOT_LAYOUT:
			node_type = "Root";
			break;
		case BLOCK_LAYOUT:
			node_type = "Block";
			break;
		case INLINE_LAYOUT:
		default:
			node_type = "Inline";
			break;
	}
	layout_node_base_t* b = &node->base_node;
	//printf("<%s> <Font: %d, %d> <Color: (%d, %d, %d)>, <Margin: %d, %d, %d, %d> <Rect: (%d, %d, %d, %d)>", node_type, r.origin.x, r.origin.y, r.size.width, r.size.height, node_type);
	_print_tabs(depth);
	printf("--- %s ", node_type);
	if (node->base_node.dom_node) {
		html_dom_node_print(node->base_node.dom_node);
	}
	printf("\n");

	_print_tabs(depth);
	printf("Font: (%d, %d)\n", b->font_size.width, b->font_size.height);
	_print_tabs(depth);
	if (b->sets_background_color) {
		printf("BG-Color: (%d, %d, %d)\n", b->background_color.val[0], b->background_color.val[1], b->background_color.val[2]);
		_print_tabs(depth);
	}
	printf("Margin: (T,B,L,R) %d, %d, %d, %d\n", b->margin_top, b->margin_bottom, b->margin_left, b->margin_right);
	_print_tabs(depth);
	Rect cr = node->base_node.content_frame;
	printf("Frame content: (x%d, y%d, w%d, h%d)\n", cr.origin.x, cr.origin.y, cr.size.width, cr.size.height);
	_print_tabs(depth);
	Rect mr = node->base_node.margin_frame;
	printf("Frame margin:  (x%d, y%d, w%d, h%d)\n", mr.origin.x, mr.origin.y, mr.size.width, mr.size.height);
	_print_tabs(depth);
	printf("MaxY content:  %d\n", rect_max_y(cr));
	_print_tabs(depth);
	printf("MaxY margin:   %d\n", rect_max_y(mr));

	//printf("<%s> <Font: %d, %d> <Color (%d): (%d, %d, %d)>, <Margin: %d, %d, %d, %d> <Rect: (%d, %d, %d, %d)> ", node_type, b->font_size.width, b->font_size.height, b->sets_background_color, b->background_color.val[0], b->background_color.val[1], b->background_color.val[2], b->margin_top, b->margin_bottom, b->margin_left, b->margin_right, r.origin.x, r.origin.y, r.size.width, r.size.height);
}

static void _print_layout_tree(layout_node_t* node, uint32_t depth) {
	_print_layout_node(node, depth);
	for (uint32_t i = 0; i < node->base_node.child_count; i++) {
		_print_layout_tree((layout_node_t*)node->base_node.children[i], depth + 1);
	}
}

// TODO(PT): Text height isn't calculated correctly

layout_root_node_t* layout_generate(html_dom_node_t* root, array_t* css_nodes, uint32_t window_width) {
    html_dom_node_t* dom_html = _html_child_tag_with_name(root, "html");
	assert(dom_html, "No <html> tag found in HTML tree\n");
    html_dom_node_t* dom_body = _html_child_tag_with_name(dom_html, "body");
	assert(dom_body, "No <body> tag found in HTML tree\n");

	printf("Creating root node..\n");
    layout_root_node_t* root_layout = layout_node_create__root_node(window_width, css_nodes);
    layout_block_node_t* body_layout = (layout_block_node_t*)_layout_node_from_dom_node((layout_node_base_t*)root_layout, dom_body, css_nodes);

	root_layout->content_frame.size.height = body_layout->margin_frame.size.height;
	root_layout->margin_frame.size.height = root_layout->content_frame.size.height + root_layout->margin_top + root_layout->margin_bottom;

	_print_layout_tree((layout_node_t*)root_layout, 0);
	return root_layout;
}
