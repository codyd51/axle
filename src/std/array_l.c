#include "array_l.h"
#include "std.h"
#include <kernel/util/mutex/mutex.h>

array_l* array_l_create() {
	array_l* ret = (array_l*)kmalloc(sizeof(array_l));
	memset(ret, 0, sizeof(array_l));

	return ret;
}

void array_l_destroy(array_l* array) {
	// This function can't be used with a locking wrapper because it frees the lock memory
	lock(&array->lock);
	array_l_item* tmp = array->head;
	while (tmp) {
		array_l_item* next = tmp->next;
		kfree(tmp);
		tmp = next;
	}
	unlock(&array->lock);
	kfree(array);
}

static type_t _array_l_lookup_unlocked(array_l* array, int32_t idx) {
	ASSERT(idx < array->size && idx >= 0, "index (%d) was out of bounds (%d)", idx, array->size - 1);

	//walk list
	array_l_item* tmp = array->head;
	for (int i = 0; i < idx; i++) {
		tmp = tmp->next;
	}
	if (tmp) {
		return tmp->item;
	}
	return NULL;
}

static void _array_l_insert_unlocked(array_l* array, type_t item) {
	printk("array_l_insert %x\n", item);

	//create container
	array_l_item* real = (array_l_item*)kmalloc(sizeof(array_l_item));
	real->item = item;
	real->next = NULL;

	//extend list
	if (array->head) {
		array_l_item* last = _array_l_lookup_unlocked(array, array->size - 1);
		printk("adding item to list, last %x\n", last);
		last->next = real;
	}
	else {
		array->head = real;
	}

	//increase size
	array->size++;
}

static int32_t _array_l_index_unlocked(array_l* array, type_t item) {
	//walk list to find item
	array_l_item* tmp = array->head;
	int idx = 0;
	while (tmp) {
		if (tmp->item == item) {
			return idx;
		}
		tmp = tmp->next;
		idx++;
	}
	//not found
	return -1;
}

static void _array_l_remove_unlocked(array_l* array, int32_t idx) {
	ASSERT(idx < array->size && idx >= 0, "can't remove object at index (%d) in array with (%d) elements", idx, array->size);

	array_l_item* tmp = array->head;
	if (!idx) {
		array->head = array->head->next;
		//kfree(tmp);
		array->size--;
		return;
	}

	//go up to element before one to remove
	for (int i = 0; i < idx - 1; i++) {
		tmp = tmp->next;
	}

	array_l_item* to_remove = tmp->next;
	/*
	if (!idx) {
		to_remove = array->head;
		array->head = array->head->next;
	}
	else {
	*/
		//set next of previous element to next of element to remove
		tmp->next = to_remove->next;
	//}

	if (!to_remove) {
		printk("array_l_remove couldn't find element to remove idx %d\n", idx);
		return;
	}

	//free container
	//kfree(to_remove);

	array->size--;
}

/*
 * Public API wrappers
 * Enforces mutex on array reads and writes
 */

void array_l_insert(array_l* array, type_t item) {
	lock(&array->lock);
	_array_l_insert_unlocked(array, item);
	unlock(&array->lock);
}

int32_t array_l_index(array_l* array, type_t item) {
	lock(&array->lock);
	int32_t ret = _array_l_index_unlocked(array, item);
	unlock(&array->lock);
	return ret;
}

void array_l_remove(array_l* array, int32_t idx) {
	lock(&array->lock);
	_array_l_remove_unlocked(array, idx);
	unlock(&array->lock);
}

type_t array_l_lookup(array_l* array, int32_t idx) {
	lock(&array->lock);
	_array_l_lookup_unlocked(array, idx);
	unlock(&array->lock);
}