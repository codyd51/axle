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

#define HORIZONTAL_STEP 10
#define VERTICAL_STEP 8

static void _layout_node_add_child(layout_node_base_t* parent, layout_node_base_t* child);
static void _layout_node_insert_child(layout_node_base_t* parent, layout_node_base_t* child, uint32_t idx);

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

static void _layout_node_clone_css(layout_node_base_t* original, layout_node_base_t* clone) {
	clone->sets_background_color = original->sets_background_color;
	clone->background_color = original->background_color;

	clone->font_size = original->font_size;
	clone->font_color = original->font_color;

	clone->margin_top = original->margin_top;
	clone->margin_right = original->margin_right;
	clone->margin_left = original->margin_left;
	clone->margin_bottom = original->margin_bottom;
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
		node->font_color = parent->font_color;
	}

	// Apply CSS styling
	for (uint32_t i = 0; i < css_nodes->size; i++) {
		css_node_t* css_node = array_lookup(css_nodes, i);

		// The root node does not have a DOM node, we'll need another way to match it to a CSS node
		if (node->mode == ROOT_LAYOUT) {
			continue;
		}
		// Anonymous block boxes also do not have DOM nodes
		if (node->is_anonymous) {
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
			if (css_node->sets_font_color) {
				node->font_color = css_node->font_color;
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
}

static layout_root_node_t* layout_node_create__root_node(uint32_t window_width, array_t* css_nodes) {
	layout_root_node_t* node = &_layout_node_alloc()->root_node;
    node->mode = ROOT_LAYOUT;
	node->is_anonymous = false;

	node->margin_top = VERTICAL_STEP;
	node->margin_bottom = VERTICAL_STEP;
	node->margin_left = HORIZONTAL_STEP;
	node->margin_right = HORIZONTAL_STEP;
	node->font_size = size_make(8, 12);
	node->font_color = color_black();

	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);

	return node;
}

static layout_block_node_t* layout_node_create__block_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes, bool is_anonymous) {
	layout_block_node_t* node = &_layout_node_alloc()->block_node;
    node->mode = BLOCK_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = dom_node;
	node->is_anonymous = is_anonymous;
	_layout_node_add_child(parent, (layout_node_base_t*)node);
	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);
	node->line_boxes = array_create(32);
	return node;
}

static layout_inline_node_t* layout_node_create__inline_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes, bool is_anonymous) {
	layout_inline_node_t* node = &_layout_node_alloc()->inline_node;
    node->mode = INLINE_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = dom_node;
	node->is_anonymous = is_anonymous;
	_layout_node_add_child(parent, (layout_node_base_t*)node);
	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);
	return node;
}

static layout_text_node_t* layout_node_create__text_node(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_text_node_t* node = &_layout_node_alloc()->text_node;
    node->mode = TEXT_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = dom_node;
	node->is_anonymous = false;
	_layout_node_add_child(parent, (layout_node_base_t*)node);
	_layout_node_apply_css_styling((layout_node_base_t*)node, css_nodes);
	return node;
}

/*
static layout_inline_node_t* layout_node_create__inline_box_subtext(layout_node_base_t* parent, layout_inline_node_t* sibling, uint32_t subtext_offset) {
	layout_inline_node_t* node = &_layout_node_alloc()->inline_node;
    node->mode = INLINE_LAYOUT;
	node->parent = (layout_node_base_t*)parent;
	node->dom_node = sibling->dom_node;
	node->is_anonymous = false;
	node->subtext_offset = subtext_offset;
	_layout_node_insert_child(parent, (layout_node_t*)node, node->idx_within_parent + 1);
	_layout_node_clone_css(sibling, node);
	return node;
}
*/

static void _layout_node_add_child(layout_node_base_t* parent, layout_node_base_t* child) {
	//printf("Add child <%s> to parent\n", child->base_node.dom_node->name);
	if (parent->child_count + 1 >= parent->max_children) {
		uint32_t new_max = parent->max_children * 2;
		printf("Resizing node's children from %d -> %d\n", parent->max_children, new_max);
		parent->children = realloc(parent->children, sizeof(parent->children[0]) * new_max);
		parent->max_children = new_max;
	}
	child->idx_within_parent = parent->child_count;
	parent->child_count += 1;
	parent->children[child->idx_within_parent] = child;
	child->parent = parent;
}

static void _layout_node_insert_child(layout_node_base_t* parent, layout_node_base_t* child, uint32_t idx) {
	//printf("Add child <%s> to parent\n", child->base_node.dom_node->name);
	//assert(idx < parent->child_count, "not implemented - just call add?");
	//if (idx < parent->child_count) {
		uint32_t elems_to_move = parent->child_count - idx;
		if (elems_to_move == 0 || true) {
			_layout_node_add_child(parent, child);
			return;
		}
		assert(elems_to_move >= 1, "Cannot insert as last elem");
		layout_node_base_t** children_off = &parent->children[idx];
		memmove(&parent->children[idx+1], &parent->children[idx], elems_to_move);
		parent->children[idx] = child;

		for (uint32_t i = 0; i < elems_to_move; i++) {
			parent->children[idx + i]->idx_within_parent += 1;
		}

		parent->child_count += 1;
	//}

	child->idx_within_parent = parent->child_count;
	parent->child_count += 1;
	parent->children[child->idx_within_parent] = child;
	child->parent = parent;
}

static layout_node_t* _layout_node_prev_sibling(layout_node_t* n) {
	// Root node?
	if (!n->base_node.parent) {
		printf("No parent\n");
		return NULL;
	}
	// First child (ie no previous sibling)?
	if (n->base_node.idx_within_parent == 0) {
		printf("idx=0\n");
		return NULL;
	}
	return (layout_node_t*)n->base_node.parent->children[n->base_node.idx_within_parent - 1];
}

static layout_mode_t _layout_mode_for_dom_node(html_dom_node_t* dom_node) {
	// There's no reason this must be the case, but we currently create the root layout node separately
	assert(dom_node->type != HTML_DOM_NODE_TYPE_DOCUMENT, "Document layout node is created separately");

	// Text DOM nodes always get text layout
	if (dom_node->type == HTML_DOM_NODE_TYPE_TEXT) {
		return TEXT_LAYOUT;
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
static layout_node_t* _layout_inline_node_from_dom_node__inline(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes);

static layout_node_t* _layout_node_from_dom_node__block(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_block_node_t* block_box = layout_node_create__block_node(parent, dom_node, css_nodes, false);

	// Scan our children to check if we have any block-level children
	// If so, if we encounter an inline child then we must wrap it in an anonymous box
	// https://www.w3.org/TR/CSS2/visuren.html#box-gen
	bool needs_anonymous_block_box_wrapping_of_inline = false;
	for (uint32_t i = 0; i < dom_node->child_count; i++) {
        html_dom_node_t* dom_child = dom_node->children[i];
		layout_mode_t child_mode = _layout_mode_for_dom_node(dom_child);
		if (child_mode == BLOCK_LAYOUT) {
			needs_anonymous_block_box_wrapping_of_inline = true;
			break;
		}
	}

	// Create layout nodes for our children
    for (uint32_t i = 0; i < dom_node->child_count; i++) {
		layout_node_base_t* parent_of_child = (layout_node_base_t*)block_box;
        html_dom_node_t* dom_child = dom_node->children[i];

		// Create an anonymous block box to contain an inline child if necessary
		layout_mode_t child_mode = _layout_mode_for_dom_node(dom_child);
		if (child_mode == INLINE_LAYOUT && needs_anonymous_block_box_wrapping_of_inline) {
			printf("\tCreated anonymous block box for inline child <%s>\n", dom_child->name);
			layout_node_base_t* anonymous_block = (layout_node_base_t*)layout_node_create__block_node((layout_node_base_t*)block_box, NULL, css_nodes, true);
			parent_of_child = anonymous_block;
		}

		// Text is always wrapped in an anonymous inline block
		// https://www.w3.org/TR/CSS2/visuren.html#anonymous-block-level:
		// "Any text that is directly contained inside a block container element 
		//  (not inside an inline element) must be treated as an anonymous inline element."
		/*
		if (child_mode == INLINE_LAYOUT && dom_child->type == HTML_DOM_NODE_TYPE_TEXT) {
			printf("\tCreated anonymous inline box for inline text <%s>\n", dom_child->name);
			layout_node_base_t* anonymous_inline = (layout_node_base_t*)layout_node_create__inline_node((layout_node_base_t*)block_box, NULL, css_nodes, true);
			anonymous_inline->is_anonymous = true;
			parent_of_child = anonymous_inline;
		}
		*/

		_layout_node_from_dom_node((layout_node_base_t*)parent_of_child, dom_child, css_nodes);
    }
	return (layout_node_t*)block_box;
}

static layout_node_t* _layout_node_from_dom_node__inline(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	layout_inline_node_t* inline_box = layout_node_create__inline_node(parent, dom_node, css_nodes, false);
	// Create layout nodes for our children
	for (uint32_t i = 0; i < dom_node->child_count; i++) {
		html_dom_node_t* dom_child = dom_node->children[i];
		layout_node_t* child = _layout_node_from_dom_node((layout_node_base_t*)inline_box, dom_child, css_nodes);
	}
	return (layout_node_t*)inline_box;
}

static layout_node_t* _layout_node_from_dom_node__text(layout_node_base_t* parent, html_dom_node_t* dom_node, array_t* css_nodes) {
	// Ensure it's really a text node
	assert(dom_node->type == HTML_DOM_NODE_TYPE_TEXT, "Expected a text node");
	// A text node should always be a leaf
	assert(dom_node->child_count == 0, "Text nodes must be leafs");
	layout_text_node_t* text_node = layout_node_create__text_node(parent, dom_node, css_nodes);
	return (layout_node_t*)text_node;
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
		case TEXT_LAYOUT:
			node = (layout_node_t*)_layout_node_from_dom_node__text(parent, dom_node, css_nodes);
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
			node_type = "Inline";
			break;
		case TEXT_LAYOUT:
			node_type = "Text";
			break;
		default:
			assert(false, "Unknown layout type");
			break;
	}
	layout_node_base_t* b = &node->base_node;

	_print_tabs(depth);
	printf("--- %s ", node_type);
	if (node->base_node.dom_node) {
		html_dom_node_print(node->base_node.dom_node);
	}
	else if (node->base_node.is_anonymous) {
		printf("(anonymous)");
	}
	printf("\n");
	_print_tabs(depth);

	if (node->inline_node.mode == TEXT_LAYOUT && node->text_node.text) {
		printf("Text: %s\n", node->text_node.text);

		_print_tabs(depth);
		printf("Font size %dx%d, color %d,%d,%d\n", b->font_size.width, b->font_size.height, b->font_color.val[0], b->font_color.val[1], b->font_color.val[2]);
		_print_tabs(depth);
	}

	if (b->sets_background_color) {
		printf("BG-Color: (%d, %d, %d)\n", b->background_color.val[0], b->background_color.val[1], b->background_color.val[2]);
		_print_tabs(depth);
	}

	if (b->margin_top || b->margin_right || b->margin_bottom || b->margin_left) {
		printf("Margin: (T,R,B,L) %d, %d, %d, %d\n", b->margin_top, b->margin_right, b->margin_bottom, b->margin_left);
		_print_tabs(depth);
	}

	Rect cr = node->base_node.content_frame;
	printf("Frame content: (x%d, y%d, w%d, h%d)\n", cr.origin.x, cr.origin.y, cr.size.width, cr.size.height);
	_print_tabs(depth);
	Rect mr = node->base_node.margin_frame;
	printf("Frame margin:  (x%d, y%d, w%d, h%d)\n", mr.origin.x, mr.origin.y, mr.size.width, mr.size.height);
	_print_tabs(depth);
	printf("MaxY content:  %d\n", rect_max_y(cr));
	_print_tabs(depth);
	printf("MaxY margin:   %d\n", rect_max_y(mr));

	if (node->base_node.mode == BLOCK_LAYOUT) {
		layout_block_node_t* block = &node->block_node;
		for (uint32_t i = 0; i < block->line_boxes->size; i++) {
			_print_tabs(depth+1);
			line_box_t* lb = array_lookup(block->line_boxes, i);
			printf("******* Line %d at (%d, %d), size (%d, %d) max_width %d)\n", i, rect_min_x(lb->frame), rect_min_y(lb->frame), lb->frame.size.width, lb->frame.size.height, lb->max_width);
			for (uint32_t j = 0; j < lb->fragments->size; j++) {
				_print_tabs(depth+2);
				line_fragment_t* lf = array_lookup(lb->fragments, j);
				printf("Line fragment %d at (%d, %d), ", j, rect_min_x(lf->frame), rect_min_y(lf->frame));
				printf("size (%d, %d), (start %d, len %d)", lf->frame.size.width, lf->frame.size.height, lf->start_idx, lf->length);
				char* val = lf->node->dom_node->name + lf->start_idx;
				printf(": %.*s\n", lf->length, val);
			}
			_print_tabs(depth+1);
			printf("******* End line boxes\n");
		}
	}

}

static void _print_layout_tree(layout_node_t* node, uint32_t depth) {
	_print_layout_node(node, depth);
	for (uint32_t i = 0; i < node->base_node.child_count; i++) {
		_print_layout_tree((layout_node_t*)node->base_node.children[i], depth + 1);
	}
}

static void layout_node_calculate_frame(layout_node_base_t* node);

static void _layout_node_calculate_frame__block(layout_node_base_t* node) {
	layout_block_node_t* block_box = (layout_block_node_t*)node;
	layout_node_base_t* parent_box = (layout_node_base_t*)block_box->parent;
	assert(parent_box, "Block box had no parent");

	// Block box starts at its parent's left content edge
	block_box->margin_frame.origin.x = rect_min_x(parent_box->content_frame);
	// And include the left margin in our content frame
	block_box->content_frame.origin.x = block_box->margin_frame.origin.x + block_box->margin_left;

	// Block boxes take up the parent's width
	uint32_t full_width = parent_box->content_frame.size.width;
	block_box->margin_frame.size.width = full_width;
	// Subtract the left and right margins of the block from its content width
	uint32_t content_width = full_width - block_box->margin_left - block_box->margin_right;
	block_box->content_frame.size.width = content_width;

	// Vertically, we start at the top of the container or just below our previous sibling
	layout_node_base_t* previous_sibling = (layout_node_base_t*)_layout_node_prev_sibling((layout_node_t*)block_box);
	if (previous_sibling) {
		// Start below the previous sibling
		uint32_t prev_max_y = rect_max_y(previous_sibling->margin_frame);
		block_box->margin_frame.origin.y = prev_max_y;
	}
	else {
		block_box->margin_frame.origin.y = rect_min_y(parent_box->content_frame);
	}

	// And add in the top margin to the content frame
	block_box->content_frame.origin.y = block_box->margin_frame.origin.y + block_box->margin_top;

	// Calculate frames recursively for our children
	for (uint32_t i = 0; i < block_box->child_count; i++) {
		layout_node_base_t* child = block_box->children[i];
		layout_node_calculate_frame(child);
	}

	// Now that we know how much space our children take up, calculate our height
	// A block should be tall enough to contain all its children
	uint32_t height_sum = 0;
	for (uint32_t i = 0; i < block_box->child_count; i++) {
		layout_node_base_t* child = block_box->children[i];
		height_sum += child->margin_frame.size.height;
	}
	block_box->content_frame.size.height = height_sum;
	// And add in the top and bottom margin to the full frame
	block_box->margin_frame.size.height = block_box->content_frame.size.height + block_box->margin_top + block_box->margin_bottom;
}

static void _layout_node_calculate_frame__inline(layout_node_base_t* node) {
	assert(false, "recheck");
	/*
	layout_inline_node_t* inline_box = (layout_inline_node_t*)node;
	layout_node_base_t* parent_box = (layout_node_base_t*)inline_box->parent;
	assert(parent_box, "Inline box had no parent");
	html_dom_node_t* dom_node = inline_box->dom_node;

	layout_node_base_t* previous_sibling = (layout_node_base_t*)_layout_node_prev_sibling((layout_node_t*)inline_box);
	printf("Previous_sibling of <%s>: <%s>\n", dom_node ? dom_node->name : "(anon.inline)", previous_sibling && previous_sibling->dom_node ? previous_sibling->dom_node->name : "");
	// Inline box starts at its previous sibling's right edge, or the parent's left origin
	// And vertically, at its previous sibling's top edge, or the parent's top edge
	if (!previous_sibling) {
		inline_box->margin_frame.origin.x = rect_min_x(parent_box->content_frame);
		inline_box->margin_frame.origin.y = rect_min_y(parent_box->content_frame);
	}
	else {
		inline_box->margin_frame.origin.x = rect_max_x(previous_sibling->margin_frame);
		inline_box->margin_frame.origin.y = rect_min_y(previous_sibling->margin_frame);

		if (inline_box->subtext_offset) {
			inline_box->margin_frame.origin.x = rect_min_x(previous_sibling->margin_frame);
			inline_box->margin_frame.origin.y = rect_max_y(previous_sibling->margin_frame);
		}
	}
	// Add in the left and top margins
	inline_box->content_frame.origin.x = rect_min_x(inline_box->margin_frame) + inline_box->margin_left;
	inline_box->content_frame.origin.y = rect_min_y(inline_box->margin_frame) + inline_box->margin_top;

	// Are we laying out raw text?
	if (dom_node && dom_node->type == HTML_DOM_NODE_TYPE_TEXT) {
		assert(dom_node->child_count == 0, "Inline text node had children?");	

		const char* dom_text = dom_node->name + inline_box->subtext_offset;
		uint32_t text_len = strlen(dom_text);
		printf("dom_text %s\n", dom_text);

		uint32_t char_width = inline_box->font_size.width;
		uint32_t char_height = inline_box->font_size.height;

		// Our width is the font width multiplied by the length of the string
		//uint32_t max_chars_per_line = parent_box->content_frame.size.width / char_width;
		uint32_t max_chars_per_line = 64;
		uint32_t max_text_width = char_width * max_chars_per_line;

		uint32_t text_width = char_width * text_len;
		//text_width = min(text_width, char_width * max_chars_per_line);

		// Cap the width to the width of the container
		//text_width = max(text_width, parent_box->content_frame.size.width);
		
		// Will we exceed the line box?
		printf("text_width %d max %d\n", text_width, max_chars_per_line * char_width);
		if (text_width > max_chars_per_line * char_width) {
			printf("Will exceed line box!\n");

			inline_box->content_frame.size.width = max_text_width;
			inline_box->margin_frame.size.width = max_text_width + inline_box->margin_left + inline_box->margin_right;

			inline_box->content_frame.size.height = char_height;
			inline_box->margin_frame.size.height = char_height + inline_box->margin_top + inline_box->margin_bottom;

			inline_box->text = strndup(dom_text, max_chars_per_line);

			printf("Creating another inline box to flow text\n");
			layout_node_create__inline_box_subtext(parent_box, inline_box, inline_box->subtext_offset + max_chars_per_line);
		}
		else {
			inline_box->content_frame.size.width = text_width;
			inline_box->margin_frame.size.width = text_width + inline_box->margin_left + inline_box->margin_right;

			inline_box->content_frame.size.height = char_height;
			inline_box->margin_frame.size.height = char_height + inline_box->margin_top + inline_box->margin_bottom;

			inline_box->text = strdup(dom_text);
		}

		// Our height is the font height multiplied by the number of lines 
		// required to display the text
		//uint32_t line_count = text_len ? (max(1, text_len / max_chars_per_line)) : 1;

		// Our height is the font height
		// TODO(PT): Create another layout node if we need to spill onto the next line
	}
	else {
		// Calculate frames recursively for our children
		for (uint32_t i = 0; i < inline_box->child_count; i++) {
			layout_node_base_t* child = inline_box->children[i];
			layout_node_calculate_frame(child);
		}

		// Now that we know how much space our children take up, calculate our height and width
		// A box should be tall and wide enough to contain all its children
		uint32_t width_sum = 0;
		uint32_t height_sum = 0;
		for (uint32_t i = 0; i < inline_box->child_count; i++) {
			layout_node_base_t* child = inline_box->children[i];
			width_sum += child->margin_frame.size.width;
			height_sum += child->margin_frame.size.height;
		}
		inline_box->content_frame.size.width = width_sum;
		// And add in the left and right margin to the full frame
		inline_box->margin_frame.size.width = inline_box->content_frame.size.width + inline_box->margin_left + inline_box->margin_right;

		inline_box->content_frame.size.height = height_sum;
		// And add in the top and bottom margin to the full frame
		inline_box->margin_frame.size.height = inline_box->content_frame.size.height + inline_box->margin_top + inline_box->margin_bottom;
	}
	*/
}

static line_box_t* _line_box_alloc(layout_block_node_t* parent_block_box) {
	line_box_t* lb = calloc(1, sizeof(line_box_t));
	lb->fragments = array_create(16);
	lb->max_width = parent_block_box->content_frame.size.width;

	// This line box will be placed below the last line box
	lb->frame.origin.x = parent_block_box->content_frame.origin.x;
	lb->frame.origin.y = parent_block_box->content_frame.origin.y;
	if (parent_block_box->line_boxes->size) {
		line_box_t* prev = array_lookup(parent_block_box->line_boxes, parent_block_box->line_boxes->size - 1);
		lb->frame.origin.y = rect_max_y(prev->frame);
	}

	for (uint32_t i = 0; i < parent_block_box->line_boxes->size; i++) {}

	array_insert(parent_block_box->line_boxes, lb);
	return lb;
}

static void _layout_text_node_generate_line_fragments(layout_node_base_t* node) {
	// TODO(PT): Might need to find the closest block parent, 
	// for example if this is text embedded in a <b> inline embedded in a <p>
	layout_text_node_t* text_node = (layout_text_node_t*)node;
	layout_block_node_t* parent_box = (layout_block_node_t*)text_node->parent;
	assert(parent_box, "Text node had no parent");
	assert(parent_box->mode == BLOCK_LAYOUT, "Expected to be within block");

	html_dom_node_t* dom_node = text_node->dom_node;
	assert(dom_node->type == HTML_DOM_NODE_TYPE_TEXT, "Expected text node");

	uint32_t next_idx_to_layout = 0;
	while (true) {
		line_box_t* last_line_box = NULL;
		if (parent_box->line_boxes->size) {
			printf("using line box %d\n", parent_box->line_boxes->size - 1);
			last_line_box = array_lookup(parent_box->line_boxes, parent_box->line_boxes->size - 1);
		}
		else {
			last_line_box = _line_box_alloc(parent_box);
		}
		Point origin = last_line_box->frame.origin;
		
		// Find the last fragment within the line box
		line_fragment_t* last_frag = NULL;
		uint32_t remaining_width = last_line_box->max_width;
		uint32_t already_placed_count = 0;

		if (last_line_box->fragments->size) {
			last_frag = array_lookup(last_line_box->fragments, last_line_box->fragments->size - 1);

			for (uint32_t i = 0; i < last_line_box->fragments->size; i++) {
				line_fragment_t* f = array_lookup(last_line_box->fragments, i);
				remaining_width -= f->frame.size.width;
				if (f->node == text_node) {
					already_placed_count += f->length;
				}
			}
			//need_to_place_start_idx = last_frag->start_idx + last_frag->length;
			origin = point_make(rect_max_x(last_frag->frame), rect_max_y(last_frag->frame));
		}

		// How much text can fit within the fragment?
		uint32_t char_width = text_node->font_size.width;
		uint32_t char_height = text_node->font_size.height;

		uint32_t placeable_chars = remaining_width / char_width;
		uint32_t need_to_place = strlen(dom_node->name) - next_idx_to_layout;
		placeable_chars = min(need_to_place, placeable_chars);

		if (!need_to_place) {
			printf("finished laying out text node\n");
			break;
		}
		if (placeable_chars == 0 && need_to_place > 0) {
			// Commit this line box by assigning its size
			last_line_box->frame.size.height = char_height;
			uint32_t width_sum = 0;
			for (uint32_t i = 0; i < last_line_box->fragments->size; i++) {
				line_fragment_t* lf = array_lookup(last_line_box->fragments, i);
				width_sum += lf->frame.size.width;
			}
			last_line_box->frame.size.width = width_sum;

			// Add a new line box that we'll use on the next loop iteration
			printf("placeable_chars = 0! need_to_place=%d start %d\n", need_to_place, next_idx_to_layout);
			_line_box_alloc(parent_box);
			continue;
		}
		printf("placable chars of <%s>: %d, need to place %d, start_idx %d\n", dom_node->name + next_idx_to_layout, placeable_chars, need_to_place, next_idx_to_layout);
		
		line_fragment_t* new_frag = calloc(1, sizeof(line_fragment_t));
		new_frag->start_idx = next_idx_to_layout;
		new_frag->length = placeable_chars;
		new_frag->node = text_node;
		new_frag->frame = rect_make(
			origin, 
			size_make(
				placeable_chars * char_width, 
				char_height
			)
		);
		array_insert(last_line_box->fragments, new_frag);

		next_idx_to_layout += placeable_chars;

			/*
		// Our width is the font width multiplied by the length of the string
		//uint32_t max_chars_per_line = parent_box->content_frame.size.width / char_width;
		uint32_t max_chars_per_line = 64;
		uint32_t max_text_width = char_width * max_chars_per_line;

		uint32_t text_width = char_width * text_len;
		//text_width = min(text_width, char_width * max_chars_per_line);
		*/
	}
}

static void layout_node_calculate_frame(layout_node_base_t* node) {
	switch (node->mode) {
		case BLOCK_LAYOUT:
			_layout_node_calculate_frame__block(node);
			break;
		case INLINE_LAYOUT:
			//_layout_node_calculate_frame__inline(node);
			//assert(false, "encountered inline node");
			break;
		case TEXT_LAYOUT:
			_layout_text_node_generate_line_fragments(node);
			break;
		default:
			assert(false, "Unknown layout type");
			break;
	}
}

static void layout_tree_calculate_frames(uint32_t window_width, layout_root_node_t* root_box) {
	assert(root_box->child_count == 1, "Expected one body child");
	layout_block_node_t* body_box = (layout_block_node_t*)root_box->children[0];

	root_box->margin_frame = rect_make(point_zero(), size_make(window_width, 0));
	root_box->content_frame = rect_make(
		point_make(
			root_box->margin_left,
			root_box->margin_top
		), 
		size_make(
			root_box->margin_frame.size.width - root_box->margin_left - root_box->margin_right, 
			0
		)
	);

	// Recursively lay out the body
	layout_node_calculate_frame((layout_node_base_t*)body_box);

	// Now that all our children have been laid out, set our height
	root_box->content_frame.size.height = body_box->margin_frame.size.height;
	root_box->margin_frame.size.height = root_box->content_frame.size.height + root_box->margin_top + root_box->margin_bottom;
}

layout_root_node_t* layout_generate(html_dom_node_t* root, array_t* css_nodes, uint32_t window_width) {
    html_dom_node_t* dom_html = _html_child_tag_with_name(root, "html");
	assert(dom_html, "No <html> tag found in HTML tree\n");
    html_dom_node_t* dom_body = _html_child_tag_with_name(dom_html, "body");
	assert(dom_body, "No <body> tag found in HTML tree\n");

	printf("Creating root node..\n");
	// Generate the layout tree
    layout_root_node_t* root_layout = layout_node_create__root_node(window_width, css_nodes);
    layout_block_node_t* body_layout = (layout_block_node_t*)_layout_node_from_dom_node((layout_node_base_t*)root_layout, dom_body, css_nodes);

	// Now, calculate frames
	layout_tree_calculate_frames(window_width, root_layout);

	_print_layout_tree((layout_node_t*)root_layout, 0);
	return root_layout;
}
