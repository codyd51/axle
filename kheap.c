#include "common.h"
#include "kheap.h"
#include "paging.h"
#include "std.h"

//end is defined in linker script
extern u32int end;
u32int placement_address = (u32int)&end;

extern page_directory_t* kernel_directory;
heap_t* kheap = 0;

u32int kmalloc_int(u32int sz, int align, u32int* phys) {
	//if addr is not already page aligned
	if (align == 1 && (placement_address & 0xFFFFF000)) {
		//align it
		placement_address &= 0xFFFFF000;
       		placement_address += 0x1000;
	}
	if (phys) {
		*phys = placement_address;
	}
	u32int tmp = placement_address;
	placement_address += sz;
	
	return tmp;
}

u32int kmalloc_a(u32int sz) {
	return kmalloc_int(sz, 1, 0);
}

u32int kmalloc_p(u32int sz, u32int* phys) {
	return kmalloc_int(sz, 0, phys);
}

u32int kmalloc_ap(u32int sz, u32int* phys) {
	return kmalloc_int(sz, 1, phys);
}

u32int kmalloc(u32int sz) {
	return kmalloc_int(sz, 0, 0);
}

static s32int find_smallest_hole(u32int size, u8int align, heap_t* heap) {
	//find smallest hole that will fit
	u32int iterator = 0;
	while (iterator < heap->index.size) {
		header_t* header = (header_t*)lookup_ordered_array(iterator, &heap->index);
		//if user has requested memory be page aligned
		if (align > 0) {
			//page align starting point of header
			u32int location = (u32int)header;
			s32int offset = 0;
			if ((location + sizeof(header_t)) & 0xFFFFF00 != 0) {
				offset = 0x1000 - (location + sizeof(header_t)) % 0x1000;
			}
			
			s32int hole_size = (s32int)header->size - offset;
			//do we still fit?
			if (hole_size >= (s32int)size) break;
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

static s8int header_t_less_than(void* a, void* b) {
	return (((header_t*)a)->size < ((header_t*)b)->size) ? 1 : 0;
}

heap_t* create_heap(u32int start, u32int end_addr, u32int max, u8int supervisor, u8int readonly) {
	heap_t* heap = (heap_t*)kmalloc(sizeof(heap_t));

	//start and end MUST be page aligned
	ASSERT(start % 0x1000 == 0);
	ASSERT(end_addr % 0x1000 == 0);

	//initialize index
	heap->index = place_ordered_array((void*)start, HEAP_INDEX_SIZE, &header_t_less_than);

	//shift start address forward to resemble where we can start putting data
	start += sizeof(type_t) * HEAP_INDEX_SIZE;

	//make sure start is still page aligned)
	if (start & 0xFFFFF000 != 0) {
		start &= 0xFFFFF000;
		start += 0x1000;
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
	insert_ordered_array((void*)hole, &heap->index);

	return heap;
}

static void expand(u32int new_size, heap_t* heap) {
	//sanity check
	ASSERT(new_size > heap->end_address - heap->start_address);
	//get nearest page boundary
	if (new_size & 0xFFFFF000 != 0) {
		new_size &= 0xFFFFF000;
		new_size += 0x1000;
	}

	//make sure we're not overreaching ourselves!
	ASSERT(heap->start_address + new_size <= heap->max_address);

	//this *should* always be on a page boundary
	u32int old_size = heap->end_address - heap->start_address;
	u32int i = old_size;
	while (i < new_size) {
		alloc_frame(get_page(heap->start_address + i, 1, kernel_directory), (heap->supervisor) ? 1 : 0, (heap->readonly) ? 0 : 1);
		i += 0x1000;
	}
	heap->end_address = heap->start_address + new_size;
}

static u32int contract(u32int new_size, heap_t* heap) {
	//sanity check
	ASSERT(new_size < heap->end_address - heap->start_address);

	//get nearest page boundary
	if (new_size & 0x1000) {
		new_size &= 0x1000;
		new_size += 0x1000;
	}

	//don't contract too far
	if (new_size < HEAP_MIN_SIZE) new_size = HEAP_MIN_SIZE;
	u32int old_size = heap->end_address - heap->start_address;
	u32int i = old_size - 0x1000;
	while (new_size < i) {
		free_frame(get_page(heap->start_address + i, 0, kernel_directory));
		i -= 0x1000;
	}
	heap->end_address = heap->start_address + new_size;
	return new_size;
}

void* alloc(u32int size, u8int align, heap_t* heap) {
	//make sure we take size of header/footer into account
	u32int new_size = size + sizeof(header_t) + sizeof(footer_t);
	//find smallest hole that will fit
	s32int iterator = find_smallest_hole(new_size, align, heap);

	if (iterator == -1) {
		//no free hole large enough was found
		
		//save some previous data
		u32int old_length = heap->end_address - heap->start_address;
		u32int old_end_address = heap->end_address;

		//we need to allocate more space
		expand(old_length + new_size, heap);
		u32int new_length = heap->end_address - heap->start_address;

		//find last header
		iterator = 0;
		//hold index of and value of endmost header found so far
		u32int idx = -1;
		u32int val = 0x0;
		while (iterator < heap->index.size) {
			u32int tmp = (u32int)lookup_ordered_array(iterator, &heap->index);
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
			insert_ordered_array((void*)header, &heap->index);
		}
		else {
			//last header needs adjusting
			header_t* header = lookup_ordered_array(idx, &heap->index);
			header->size += new_length - old_length;

			//rewrite footer
			footer_t* footer = (footer_t*)((u32int)header + header->size - sizeof(footer_t));
			footer->header = header;
			footer->magic = HEAP_MAGIC;
		}

		//we should now have enough space
		//try allocation again
		return alloc(size, align, heap);
	}

	header_t* orig_hole_header = (header_t*)lookup_ordered_array(iterator, &heap->index);
	u32int orig_hole_pos = (u32int)orig_hole_header;
	u32int orig_hole_size = orig_hole_header->size;

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
		u32int new_location = orig_hole_pos + 0x1000 - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		header_t* hole_header = (header_t*)orig_hole_pos;
		hole_header->size = 0x1000 - (orig_hole_pos & 0xFFF) - sizeof(header_t);
		hole_header->magic = HEAP_MAGIC;
		hole_header->hole = 1;
		footer_t* hole_footer = (footer_t*)((u32int)new_location - sizeof(footer_t));
		hole_footer->magic = HEAP_MAGIC;
		hole_footer->header = hole_header;
		orig_hole_pos = new_location;
		orig_hole_size = orig_hole_size - hole_header->size;
	}
	else {
		//we don't need this hole any more, delete it from index
		remove_ordered_array(iterator, &heap->index);
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
		
		footer_t* hole_footer = (footer_t*)((u32int)hole_header + orig_hole_size - new_size - sizeof(footer_t));
		if ((u32int)hole_footer < heap->end_address) {
			hole_footer->magic = HEAP_MAGIC;
			hole_footer->header = hole_header;
		}

		//put new hole in index
		insert_ordered_array((void*)hole_header, &heap->index);
	}

	return (void*)((u32int)block_header + sizeof(header_t));
}

void free(void* p, heap_t* heap) {
	if (p == 0) return;

	//get header and footer associated with this pointer
	header_t* header = (header_t*)((u32int)p - sizeof(header_t));
	footer_t* footer = (footer_t*)((u32int)header + header->size - sizeof(footer_t));

	//ensure these are valid
	ASSERT(header->magic == HEAP_MAGIC);
	ASSERT(footer->magic == HEAP_MAGIC);

	//turn this into a hole
	header->hole = 1;

	//determine if we should add this header into free holes index
	char add = 1;

	//attempt merge left
	//if thing to left of us is a footer...
	footer_t* test_footer = (footer_t*)((u32int)header - sizeof(footer_t));
	if (test_footer->magic == HEAP_MAGIC && test_footer->header->hole == 1) {
		u32int cache_size = header->size; //cache current size
		header = test_footer->header; //rewrite header with new one
		footer->header = header; //rewrite footer to point to new header
		header->size += cache_size; //change size
		add = 0; //since header is already in index, don't add it again
	}

	//attempt merge right
	//if thing to right of us is a header...
	header_t* test_header = (header_t*)((u32int)footer + sizeof(footer_t));
	if (test_header->magic == HEAP_MAGIC && test_header->hole == 1) {
		header->size += test_header->size; //increase size to fit merged hole
		test_footer = (footer_t*)((u32int)test_header + test_header->size - sizeof(footer_t)); //rewrite its footer to point to our header
		footer = test_footer;

		//find and remove this header from index
		u32int iterator = 0;
		while ((iterator < heap->index.size) && (lookup_ordered_array(iterator, &heap->index) != (void*)test_header)) {
			iterator++;
		}

		//ensure we actually found the item
		ASSERT(iterator < heap->index.size);
		//remove it
		remove_ordered_array(iterator, &heap->index);
	}

	//if footer location is the end address, we can contract
	if ((u32int)footer + sizeof(footer_t) == heap->end_address) {
		u32int old_length = heap->end_address - heap->start_address;
		u32int new_length = contract((u32int)header - heap->start_address, heap);
		//check how big we'll be after resizing
		if (header->size - (old_length - new_length) > 0) {
			//we still exist, so resize us
			header->size -= old_length - new_length;
			footer = (footer_t*)((u32int)header + header->size - sizeof(footer_t));
			footer->magic = HEAP_MAGIC;
			footer->header = header;
		}
		else {
			//we no longer exist
			//remove us from index
			u32int iterator = 0;
			while ((iterator < heap->index.size) && (lookup_ordered_array(iterator, &heap->index) != (void*)test_header)) {
				iterator++;
			}

			//if we didn't find ourselves, we have nothing to remove
			if (iterator < heap->index.size) {
				remove_ordered_array(iterator, &heap->index);
			}
		}
	}

	if (add = 1) {
		insert_ordered_array((void*)header, &heap->index);
	}
}

