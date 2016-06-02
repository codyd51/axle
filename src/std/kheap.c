#include "common.h"
#include "kheap.h"
#include <kernel/util/paging/paging.h>
#include "std.h"
#include <std/math.h>

#define PAGE_SIZE 0x1000 /* 4kb page */

//end is defined in linker script
extern uint32_t end;
uint32_t placement_address = (uint32_t)&end;

extern page_directory_t* kernel_directory;
extern page_directory_t* current_directory;

heap_t* kheap = 0;

uint32_t kmalloc_int(uint32_t sz, int align, uint32_t* phys) {
	//if the heap already exists, pass through
	if (kheap) {
		void* addr = alloc(sz, (uint8_t)align, kheap);
		if (phys) {
			page_t* page = get_page((uint32_t)addr, 0, kernel_directory);
			*phys = page->frame * PAGE_SIZE + ((uint32_t)addr & 0xFFF);
		}
		return (uint32_t)addr;
	}
	//if addr is not already page aligned
	if (align == 1 && (placement_address & 0xFFFFF000)) {
		//align it
		placement_address &= 0xFFFFF000;
       		placement_address += PAGE_SIZE;
	}
	if (phys) {
		*phys = placement_address;
	}
	uint32_t tmp = placement_address;
	placement_address += sz;
	
	return tmp;
}

uint32_t kmalloc_a(uint32_t sz) {
	return kmalloc_int(sz, 1, 0);
}

uint32_t kmalloc_p(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 0, phys);
}

uint32_t kmalloc_ap(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 1, phys);
}

uint32_t kmalloc(uint32_t sz) {
	return kmalloc_int(sz, 0, 0);
}

void kfree(void* p) {
	free(p, kheap);
}

static int32_t find_smallest_hole(uint32_t size, uint8_t align, heap_t* heap) {
	//find smallest hole that will fit
	uint32_t iterator = 0;
	while (iterator < heap->index.size) {
		header_t* header = (header_t*)array_o_lookup(iterator, &heap->index);

		//check if magic is valid
		ASSERT(header->magic == HEAP_MAGIC, "invalid header magic");

		//if user has requested memory be page aligned
		//
		if (align > 0) {
			//page align starting point of header
			uint32_t location = (uint32_t)header;
			int32_t offset = 0;
			if ((location + sizeof(header_t) & 0xFFFFF000) != 0) {
				offset = PAGE_SIZE - ((location + sizeof(header_t)) % PAGE_SIZE);
			}
			
			int32_t hole_size = (int32_t)header->size - offset;
			//do we still fit?
			if (hole_size >= (int32_t)size) break;
		}
		else if (header->size >= size) break;
		
		iterator++;
	}

	//why did the loop exit?
	if (iterator == heap->index.size) {
		//reached end of index and didn't find any holes small enough
		return -1;
	}
	
	return iterator;
}

static int8_t header_t_less_than(void* a, void* b) {
	return (((header_t*)a)->size < ((header_t*)b)->size) ? 1 : 0;
}

heap_t* create_heap(uint32_t start, uint32_t end_addr, uint32_t max, uint8_t supervisor, uint8_t readonly) {
	heap_t* heap = (heap_t*)kmalloc(sizeof(heap_t));

	//start and end MUST be page aligned
	ASSERT(start % PAGE_SIZE == 0, "start wasn't page aligned");
	ASSERT(end_addr % PAGE_SIZE == 0, "end_addr wasn't page aligned");

	//initialize index
	heap->index = array_o_place((void*)start, HEAP_INDEX_SIZE, &header_t_less_than);

	//shift start address forward to resemble where we can start putting data
	start += sizeof(type_t) * HEAP_INDEX_SIZE;

	//make sure start is still page aligned)
	if ((start & 0xFFFFF000) != 0) {
		start &= 0xFFFFF000;
		start += PAGE_SIZE;
	}

	//write start, end, and max addresses into heap structure
	heap->start_address = start;
	heap->end_address = end_addr;
	heap->max_address = max;
	heap->supervisor = supervisor;
	heap->readonly = readonly;

	//we start off with one large hole in the index
	//this represents the whole heap at this point
	header_t* hole = (header_t*)start;
	hole->size = end_addr - start;
	hole->magic = HEAP_MAGIC;
	hole->hole = 1;
	array_o_insert((void*)hole, &heap->index);

	return heap;
}

void expand(uint32_t new_size, heap_t* heap) {
	//sanity check
	ASSERT(new_size > heap->end_address - heap->start_address, "new_size was larger than heap");
	//get nearest page boundary
	if ((new_size & 0xFFFFF000) != 0) {
		new_size &= 0xFFFFF000;
		new_size += PAGE_SIZE;
	}

	//make sure we're not overreaching ourselves!
	ASSERT(heap->start_address + new_size <= heap->max_address, "heap would exceed max capacity");

	//this *should* always be on a page boundary
	uint32_t old_size = heap->end_address - heap->start_address;
	uint32_t i = old_size;
	while (i < new_size) {
		alloc_frame(get_page(heap->start_address + i, 1, current_directory == 0 ? kernel_directory : current_directory), (heap->supervisor) ? 1 : 0, (heap->readonly) ? 0 : 1);
		i += PAGE_SIZE;
	}
	heap->end_address = heap->start_address + new_size;
}

static uint32_t contract(uint32_t new_size, heap_t* heap) {
	//sanity check
	ASSERT(new_size < heap->end_address - heap->start_address, "new_size was larger than heap");

	//get nearest page boundary
	if (new_size & PAGE_SIZE) {
		new_size &= PAGE_SIZE;
		new_size += PAGE_SIZE;
	}

	//don't contract too far
	new_size = MAX(new_size, HEAP_MIN_SIZE);
	
	uint32_t old_size = heap->end_address - heap->start_address;
	uint32_t i = old_size - PAGE_SIZE;
	while (new_size < i) {
		free_frame(get_page(heap->start_address + i, 0, current_directory == 0 ? kernel_directory : current_directory));
		i -= PAGE_SIZE;
	}
	heap->end_address = heap->start_address + new_size;
	return new_size;
}

void* alloc(uint32_t size, uint8_t align, heap_t* heap) {
	//make sure we take size of header/footer into account
	uint32_t new_size = size + sizeof(header_t) + sizeof(footer_t);
	//find smallest hole that will fit
	int32_t iterator = find_smallest_hole(new_size, align, heap);

	if (iterator == -1) {
		//no free hole large enough was found
		
		//save some previous data
		uint32_t old_length = heap->end_address - heap->start_address;
		uint32_t old_end_address = heap->end_address;

		//we need to allocate more space
		expand(old_length + new_size, heap);
		uint32_t new_length = heap->end_address - heap->start_address;

		//find last header
		iterator = 0;
		//hold index of and value of endmost header found so far
		uint32_t idx = -1;
		uint32_t val = 0x0;
		while (iterator < heap->index.size) {
			uint32_t tmp = (uint32_t)array_o_lookup(iterator, &heap->index);
			if (tmp > val) {
				val = tmp;
				idx = iterator;
			}
			iterator++;
		}

		//if we didn't find any headers, add one
		if (idx == -1) {
			header_t* header = (header_t*)old_end_address;
			header->magic = HEAP_MAGIC;
			header->hole = 1;
			header->size = new_length - old_length;
			
			footer_t* footer = (footer_t*)(old_end_address + header->size - sizeof(footer_t));
			footer->magic = HEAP_MAGIC;
			footer->header = header;
			array_o_insert((void*)header, &heap->index);
		}
		else {
			//last header needs adjusting
			header_t* header = array_o_lookup(idx, &heap->index);
			header->size += new_length - old_length;

			//rewrite footer
			footer_t* footer = (footer_t*)((uint32_t)header + header->size - sizeof(footer_t));
			footer->header = header;
			footer->magic = HEAP_MAGIC;
		}

		//we should now have enough space
		//try allocation again
		return alloc(size, align, heap);
	}

	header_t* orig_hole_header = (header_t*)array_o_lookup(iterator, &heap->index);
	uint32_t orig_hole_pos = (uint32_t)orig_hole_header;
	uint32_t orig_hole_size = orig_hole_header->size;

	//check if we should split hole into 2 parts
	//this is only worth it if the new hole's size is greater than the 
	//size we need to store the header and footer
	if (orig_hole_size - new_size < sizeof(header_t) + sizeof(footer_t)) {
		//increase size to size of hole we found
		size += orig_hole_size - new_size;
		new_size = orig_hole_size;
	}

	//if it needs to be page aligned, do it now and
	//make a new hole in front of our block
	if (align && orig_hole_pos & 0xFFFFF000) {
		uint32_t new_location = orig_hole_pos + PAGE_SIZE - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		header_t* hole_header = (header_t*)orig_hole_pos;
		hole_header->size = PAGE_SIZE - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		hole_header->magic = HEAP_MAGIC;
		hole_header->hole = 1;
		footer_t* hole_footer = (footer_t*)((uint32_t)new_location - sizeof(footer_t));
		hole_footer->magic = HEAP_MAGIC;
		hole_footer->header = hole_header;
		orig_hole_pos = new_location;
		orig_hole_size = orig_hole_size - hole_header->size;
	}
	else {
		//we don't need this hole any more, delete it from index
		array_o_remove(iterator, &heap->index);
	}

	//overwrite original header
	header_t* block_header = (header_t*)orig_hole_pos;
	block_header->magic = HEAP_MAGIC;
	block_header->hole = 0;
	block_header->size = new_size;
	//and overwrite footer
	footer_t* block_footer = (footer_t*)(orig_hole_pos + sizeof(header_t) + size);
	block_footer->magic = HEAP_MAGIC;
	block_footer->header = block_header;

	//we might have to write a new hole after the allocated block
	//only do this if the new hole would have a positive size after
	//subtracting size needed for header and footer
	if (orig_hole_size - new_size > 0) {
		header_t* hole_header = (header_t*)(orig_hole_pos + sizeof(header_t) + size + sizeof(footer_t));
		hole_header->magic = HEAP_MAGIC;
		hole_header->hole = 1;
		hole_header->size = orig_hole_size - new_size;
		
		footer_t* hole_footer = (footer_t*)((uint32_t)hole_header + orig_hole_size - new_size - sizeof(footer_t));
		if ((uint32_t)hole_footer < heap->end_address) {
			hole_footer->magic = HEAP_MAGIC;
			hole_footer->header = hole_header;
		}

		//put new hole in index
		array_o_insert((void*)hole_header, &heap->index);
	}

	return (void*)((uint32_t)block_header + sizeof(header_t));
}

void free(void* p, heap_t* heap) {
	if (p == 0) return;

	//get header and footer associated with this pointer
	header_t* header = (header_t*)((uint32_t)p - sizeof(header_t));
	footer_t* footer = (footer_t*)((uint32_t)header + header->size - sizeof(footer_t));

	//ensure these are valid
	ASSERT(header->magic == HEAP_MAGIC, "invalid header magic");
	ASSERT(footer->magic == HEAP_MAGIC, "invalid footer magic");

	//turn this into a hole
	header->hole = 1;

	//determine if we should add this header into free holes index
	char add = 1;

	//attempt merge left
	//if thing to left of us is a footer...
	footer_t* test_footer = (footer_t*)((uint32_t)header - sizeof(footer_t));
	if (test_footer->magic == HEAP_MAGIC && test_footer->header->hole == 1) {
		uint32_t cache_size = header->size; //cache current size
		header = test_footer->header; //rewrite header with new one
		footer->header = header; //rewrite footer to point to new header
		header->size += cache_size; //change size
		add = 0; //since header is already in index, don't add it again
	}

	//attempt merge right
	//if thing to right of us is a header...
	header_t* test_header = (header_t*)((uint32_t)footer + sizeof(footer_t));
	if (test_header->magic == HEAP_MAGIC && test_header->hole == 1) {
		header->size += test_header->size; //increase size to fit merged hole
		test_footer = (footer_t*)((uint32_t)test_header + test_header->size - sizeof(footer_t)); //rewrite its footer to point to our header
		footer = test_footer;

		//find and remove this header from index
		uint32_t iterator = 0;
		while ((iterator < heap->index.size) && (array_o_lookup(iterator, &heap->index) != (void*)test_header)) {
			iterator++;
		}

		//ensure we actually found the item
		ASSERT(iterator < heap->index.size, "couldn't find item!");

		//header is fake, delete it and process as if there was nothing
		if (iterator > heap->index.size) {
			//delete fake header
			test_header->magic = 0;
			test_header->hole = 1;
		}
		else {
			//everything was normal
			//increase size
			header->size += test_header->size;
			test_footer = (footer_t*)((uint32_t)test_header + test_header->size - sizeof(footer_t));
			footer = test_footer;
			//remove it
			array_o_remove(iterator, &heap->index);
		}
	}

	//if footer location is the end address, we can contract
	if ((uint32_t)footer + sizeof(footer_t) == heap->end_address) {
		uint32_t old_length = heap->end_address - heap->start_address;
		uint32_t new_length = contract((uint32_t)header - heap->start_address, heap);
		//check how big we'll be after resizing
		if (header->size - (old_length - new_length) > 0) {
			//we still exist, so resize us
			header->size -= old_length - new_length;
			footer = (footer_t*)((uint32_t)header + header->size - sizeof(footer_t));
			footer->magic = HEAP_MAGIC;
			footer->header = header;
		}
		else {
			//we no longer exist
			//remove us from index
			uint32_t iterator = 0;
			while ((iterator < heap->index.size) && (array_o_lookup(iterator, &heap->index) != (void*)test_header)) {
				iterator++;
			}

			//if we didn't find ourselves, we have nothing to remove
			if (iterator < heap->index.size) {
				array_o_remove(iterator, &heap->index);
			}
		}
	}

	if (add = 1) {
		array_o_insert((void*)header, &heap->index);
	}
}

