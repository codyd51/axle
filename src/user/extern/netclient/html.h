#ifndef HTML_H
#define HTML_H

#include <stdint.h>
#include "shims.h"

typedef enum html_dom_node_type {
	HTML_DOM_NODE_TYPE_DOCUMENT = 0,
	HTML_DOM_NODE_TYPE_HTML_TAG = 1,
	HTML_DOM_NODE_TYPE_TEXT = 2,
} html_dom_node_type_t;

typedef struct html_dom_node {
	html_dom_node_type_t type;
	char* name;
	char* attrs;

	struct html_dom_node* parent;
	struct html_dom_node** children;
	uint32_t max_children;
	uint32_t child_count;
} html_dom_node_t;

typedef struct html_dom {
	html_dom_node_t* root;
} html_dom_t;

html_dom_node_t* html_parse_from_socket(uint32_t conn_desc);
void html_dom_node_print(html_dom_node_t* node);

#endif