#ifndef STD_KHEAP_H
#define STD_KHEAP_H

#include "std_base.h"
#include "ordered_array.h"
#include <stdint.h>

__BEGIN_DECLS

//page aligned
STDAPI uint32_t kmalloc_a(uint32_t sz);

//returns physical address
STDAPI uint32_t kmalloc_p(uint32_t sz, uint32_t* phys);

//page aligned and returns physical address
STDAPI uint32_t kmalloc_ap(uint32_t sz, uint32_t* phys);

//normal kmalloc
STDAPI uint32_t kmalloc(uint32_t sz);

#define KHEAP_START		0x40000000
#define KHEAP_INITIAL_SIZE	0x1000000
#define KHEAP_MAX_ADDRESS	0xFFFF000

#define HEAP_INDEX_SIZE		0x20000
#define HEAP_MAGIC		0x25A56F9
#define HEAP_MIN_SIZE		0x70000

//size information for hole/block
typedef struct {
	uint32_t magic; //magic number
	uint8_t hole; //block or hole?
	uint32_t size; //size, including end footer
} header_t;

typedef struct {
	uint32_t magic; 
	header_t* header; //reference to header
} footer_t;

typedef struct {
	ordered_array_t index;
	uint32_t start_address; //start of allocated space
	uint32_t end_address; //end of allocated space (can be expanded up to max_address)
	uint32_t max_address; //maximum address heap can be expanded to
	uint8_t supervisor; //should new pages mapped be marked as kernel mode?
	uint8_t readonly; //should new pages mapped be marked as read-only?
} heap_t;

//create new heap
STDAPI heap_t* create_heap(uint32_t start, uint32_t end, uint32_t max, uint8_t supervisor, uint8_t readonly);

//allocates contiguous region of memory of size 'size'. If aligned, creates block starting on page boundary
STDAPI void* alloc(uint32_t size, uint8_t page_align, heap_t* heap);

//releases block allocated with alloc
STDAPI void free(void* p, heap_t* heap);

//releases block allocated with alloc using current heap
STDAPI void kfree(void* p);

__END_DECLS

#endif // STD_KHEAP_H
