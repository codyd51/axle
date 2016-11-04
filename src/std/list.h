#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stdint.h>
#include "list_node.h"

//basic dynamic list
typedef struct list_s {
	unsigned int size; //number of items in list
	list_node* head; //root node in list
} list;

//list constructor
list* list_create();
//insert at end of list
//returns false if insertion failed
bool list_add(list* list, void* payload);
//get payload at index idx
void* list_get(list* list, uint32_t idx);

#endif
