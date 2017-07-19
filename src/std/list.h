#ifndef LIST_H
#define LIST_H

#include "listnode.h"

//================| List Class Declaration |================//

//A type to encapsulate a basic dynamic list
typedef struct List_struct {
    unsigned int count; 
    ListNode* root_node;
} List;

//Methods
List* List_new();
int List_add(List* list, void* payload);
void* List_get_at(List* list, unsigned int index);
void* List_remove_at(List* list, unsigned int index);

#endif //LIST_H

/*
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
*/
