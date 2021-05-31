#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include "shims.h"

typedef struct css_node {
	char* name;

	bool sets_background_color;
	Color background_color;

	bool sets_margin_top;
	uint32_t margin_top;

	bool sets_margin_bottom;
	uint32_t margin_bottom;

	bool sets_margin_left;
	uint32_t margin_left;

	bool sets_margin_right;
	uint32_t margin_right;

	bool sets_font_size_em;
	uint32_t font_size_em;

	bool sets_font_color;
	Color font_color;

	struct css_node* parent;
	struct css_node** children;
	uint32_t max_children;
	uint32_t child_count;
} css_node_t;

array_t* css_parse(const char* stylesheet);
void css_tree_print(css_node_t* root);

#endif