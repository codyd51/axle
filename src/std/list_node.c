#include "list_node.h"
#include "kheap.h"
#include "memory.h"

list_node* list_node_create(void* payload) {
	//malloc or fail
	list_node* node;
	if (!(node = kmalloc(sizeof(list_node)))) {
		return node;
	}

	//initial vals
	memset(node, 0, sizeof(list_node));
	node->payload = payload;
	return node;
}
