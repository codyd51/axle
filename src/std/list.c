#include "list.h"
#include "kheap.h"
#include "std.h"

list* list_create() {
	//malloc or fail
	list* list;
	if (!(list = kmalloc(sizeof(list)))) {
		return list;
	}

	//fill initial vals
	list->size = 0;
	list->head = (list_node*)NULL;

	return list;
}

//insert at end of list
bool list_add(list* list, void* payload) {
	//make node or fail
	list_node* node;
	if (!(node = list_node_create(payload))) {
		return false;
	}

	//if no items in list, set head
	if (!list->head) {
		list->head = node;
	}
	else {
		//otherwise, find last node
		list_node* current = list->head;
		while (current->next) {
			current = current->next;
		}

		//make last & first point to each other
		current->next = node;
		node->prev = current;
	}

	//update number of items in list
	list->size++;
	return true;
}

//get payload at index
void* list_get(list* list, uint32_t idx) {
	//if nothing in list or requesting beyond list bounds, error
	if (!list->size || idx >= list->size) {
		return NULL;
	}

	//iterate items in list until we get to idx
	list_node* current = list->head;
	for (uint32_t i = 0; (i < idx) && current; i++) {
		current = current->next;
	}

	//guard against bad lists
	return (current ? current->payload : (void*)0);
}
