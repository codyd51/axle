#include "common.h"
#include "ordered_array.h"

//page aligned
u32int kmalloc_a(u32int sz);
//returns physical address
u32int kmalloc_p(u32int sz, u32int* phys);
//page aligned and returns physical address
u32int kmalloc_ap(u32int sz, u32int* phys);
//normal kmalloc
u32int kmalloc(u32int sz);

#define KHEAP_START		0xC0000000
#define KHEAP_INITIAL_SIZE	0x100000
#define HEAP_INDEX_SIZE		0x20000
#define HEAP_MAGIC		0x25A56F9
#define HEAP_MIN_SIZE		0x70000

//size information for hole/block
typedef struct {
	u32int magic; //magic number
	u8int hole; //block or hole?
	u32int size; //size, including end footer
} header_t;

typedef struct {
	u32int magic; 
	header_t* header; //reference to header
} footer_t;

typedef struct {
	ordered_array_t index;
	u32int start_address; //start of allocated space
	u32int end_address; //end of allocated space (can be expanded up to max_address)
	u32int max_address; //maximum address heap can be expanded to
	u8int supervisor; //should new pages mapped be marked as kernel mode?
	u8int readonly; //should new pages mapped be marked as read-only?
} heap_t;

//create new heap
heap_t* create_heap(u32int start, u32int end, u32int max, u8int supervisor, u8int readonly);

//allocates contiguous region of memory of size 'size'. If aligned, creates block starting on page boundary
void* alloc(u32int size, u8int page_align, heap_t* heap);

//releases block allocated with alloc
void free(void* p, heap_t* heap);

