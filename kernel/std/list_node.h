#ifndef LIST_NODE_H
#define LIST_NODE_H

//encapsulates item in linked list
typedef struct list_node_s {
	void* payload; //contents of node
	struct list_node_s* prev; //back node
	struct list_node_s* next; //next node
} list_node;

list_node* list_node_create(void* payload);

#endif
