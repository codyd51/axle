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

static type_t _array_l_lookup_unlocked(array_l* array, int32_t idx, array_l_item** out) {
	ASSERT(idx < array->size && idx >= 0, "index (%d) was out of bounds (%d)", idx, array->size - 1);

	//walk list
	array_l_item* tmp = array->head;
	for (int i = 0; i < idx; i++) {
		tmp = tmp->next;
	}
	if (tmp) {
		if (out) {
			*out = tmp;
		}
		return tmp->item;
	}
	return NULL;
}

static void _array_l_insert_unlocked(array_l* array, type_t item) {
	//printk("array_l_insert(0x%08x, 0x%08x) (curr size %d)\n", array, item, array->size);

	//create container
	array_l_item* real = (array_l_item*)kmalloc(sizeof(array_l_item));
	real->item = item;
	real->next = NULL;

	//extend list
	if (array->head) {
		array_l_item* last = 0;
		_array_l_lookup_unlocked(array, array->size - 1, &last);
		assert(last, "Failed to find container for last list entry");
		//printk("  Found last elem 0x%08x\n", last);
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
		array->size--;
		kfree(tmp);
		return;
	}

	// Find the element just prior to the element to delete
	for (int i = 0; i < idx - 1; i++) {
		tmp = tmp->next;
	}

	array_l_item* to_remove = tmp->next;
	if (!to_remove) {
		printk("array_l_remove couldn't find element to remove idx %d\n", idx);
		return;
	}

	// Move its `next` pointer to the element just after the element to delete
	tmp->next = to_remove->next;

	// Free entry container
	kfree(to_remove);

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
	type_t ret = _array_l_lookup_unlocked(array, idx, NULL);
	unlock(&array->lock);
	return ret;
}