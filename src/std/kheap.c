#include "kheap.h"
#include <kernel/util/pmm/pmm.h>
#include <kernel/util/vmm/vmm.h>

static void alloc_chunk(uint32_t start, uint32_t size);
static void free_chunk(heap_header_t* chunk);
static void split_chunk(heap_header_t* chunk, uint32_t size);
static void absorb_chunk(heap_header_t* chunk);

uint32_t heap_max = HEAP_START;
heap_header_t* heap_first = 0;

void heap_init() {}

void* kmalloc(uint32_t size) {
	size += sizeof(heap_header_t);

	heap_header_t* curr = heap_first;
	heap_header_t* prev = 0;
	while (curr) {
		if (!curr->allocated && curr->size >= size) {
			split_chunk(curr, size);
			curr->allocated = true;
			return (void*)((uint32_t)curr + sizeof(heap_header_t));
		}
		prev = curr;
		curr = curr->next;
	}

	uint32_t chunk_start;
	if (prev) {
		chunk_start = (uint32_t)prev + prev->size;
	}
	else {
		chunk_start = HEAP_START;
		heap_first = (heap_header_t*)chunk_start;
	}

	alloc_chunk(chunk_start, size);
	curr = (heap_header_t*)chunk_start;
	curr->prev = prev;
	curr->next = 0;
	curr->allocated = true;
	curr->size = size;

	prev->next = curr;

	return (void*)(chunk_start + sizeof(heap_header_t));
}

void kfree(void* p) {
	heap_header_t* header = (heap_header_t*)((uint32_t)p - sizeof(heap_header_t));
	header->allocated = false;

	absorb_chunk(header);
}

void alloc_chunk(uint32_t start, uint32_t size) {
	while (start + size > heap_max) {
		uint32_t page = pmm_alloc_page();
		vmm_map(heap_max, page, PAGE_PRESENT | PAGE_WRITE);
		heap_max += PAGE_SIZE;
	}
}

void free_chunk(heap_header_t* chunk) {
	chunk->prev->next = 0;
	if (!chunk->prev) {
		heap_first = 0;
	}

	//while heap max can contract by a page size and still be greater than chunk addr
	while ((heap_max - PAGE_SIZE) >= (uint32_t)chunk) {
		heap_max -= PAGE_SIZE;
		uint32_t page;
		vmm_get_mapping(heap_max, &page);
		pmm_free_page(page);
		vmm_unmap(heap_max);
	}
}

void split_chunk(heap_header_t* chunk, uint32_t size) {
	//before splitting a chunk, ensure the resultant size can at least fit a chunk header
	//otherwise, not worthwhile
	if (chunk->size - size > sizeof(heap_header_t)) {
		heap_header_t* new = (heap_header_t*)((uint32_t)chunk + chunk->size);
		new->prev = chunk;
		new->next = 0;
		new->allocated = 0;
		new->size = chunk->size - size;

		chunk->next = new;
		chunk->size = size;
	}
}

void absorb_chunk(heap_header_t* chunk) {
	if (chunk->next && !chunk->next->allocated) {
		chunk->size += chunk->next->size;
		chunk->next->next->prev = chunk;
		chunk->next = chunk->next->next;
	}

	if (chunk->prev && !chunk->prev->allocated) {
		chunk->prev->size += chunk->size;
		chunk->prev->next = chunk->next;
		chunk->next->prev = chunk->prev;
		chunk = chunk->prev;
	}

	if (!chunk->next) {
		free_chunk(chunk);
	}
}
