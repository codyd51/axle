#include "common.h"
#include "kheap.h"
#include <kernel/util/paging/paging.h>
#include "std.h"
#include <std/math.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/interrupts/interrupts.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/assert.h>
#include <kernel/vmm/vmm.h>
#include <kernel/boot_info.h>

heap_t kheap_raw = {0};
heap_t* kheap = &kheap_raw;
static uint32_t used_bytes;

void* kmalloc_int(uint32_t sz, int align, uint32_t* phys);
void kfree(void* p);
void heap_fail(void* dump);

//root kmalloc function
//, pass through to heap with given options
static void* _kmalloc_int_unlocked(uint32_t sz, int align, uint32_t* phys) {
	//if the heap already exists, pass through
	if (kheap->start_address) {
		void* addr = alloc(sz, (uint8_t)align, kheap);
		if (phys) {
            *phys = vmm_get_phys_for_virt(addr);
		}
		return addr;
	}
    panic("kmalloc_int called before heap was alive.");
    return NULL;
}

void* kmalloc_a(uint32_t sz) {
	return kmalloc_int(sz, 1, 0);
}

void* kmalloc_p(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 0, phys);
}

void* kmalloc_ap(uint32_t sz, uint32_t* phys) {
	return kmalloc_int(sz, 1, phys);
}

void* kmalloc_real(uint32_t sz) {
	return kmalloc_int(sz, 0, 0);
}

void _kfree_unlocked(void* p) {
	uint32_t addr = (uint32_t)p;
	if (addr <= kheap->start_address || addr >= kheap->end_address) {
		printf("kfree() invalid block 0x08%x\n", addr);
		panic("kfree() invalid block\n");
	}
	free(p, kheap);
}

//create a heap header at `addr`, with the associated block being `size` bytes large
static alloc_block_t* create_block(uint32_t addr, uint32_t size) {
	alloc_block_t* block = (alloc_block_t*)addr;
	memset(block, 0, sizeof(alloc_block_t));
	block->magic = HEAP_MAGIC;
	block->free = true;
	block->size = size;
	return block;
}

//insert new block header into linked list of blocks
//inserts in space past prev
static void insert_block(alloc_block_t* prev, alloc_block_t* new) {
	if (!prev || !new) {
		printk_err("insert_block(): prev or new was NULL");
		return;
	}

	if (prev->next) {
		new->next = prev->next;
	}
	if (new->next) {
		new->next->prev = new;
	}

	prev->next = new;
	new->prev = prev;
}

//get the first block header in linked list
static alloc_block_t* first_block(heap_t* heap) {
	return (alloc_block_t*)heap->start_address;
}

//find the smallest block at least size bytes big, and,
//if page aligning is requested, is large enough to be page aligned
//(if so, page-aligns block and returns aligned block)
static alloc_block_t* find_smallest_hole(uint32_t size, bool align, heap_t* heap) {
	//printf_info("find_smallest_hole(): %x bytes align? %d", size, align);
	// Search each memory block
	alloc_block_t* candidate = first_block(heap);
	do {
		ASSERT(candidate->magic == HEAP_MAGIC, "find_smallest_hole() detected heap corruption");
		if (!candidate->free) {
			continue;
		}
		if (candidate->size < size) {
			continue;
		}
		// Found a free block large enough to fit the alloc

		//attempt to align if user requested
		//make sure addr isn't already page aligned before aligning
		uint32_t addr = (uint32_t)candidate + sizeof(alloc_block_t);
		if (align && (addr & PAGE_FLAG_BITS_MASK)) {
			//Deprecated();
			//find distance to page align
			uint32_t aligned_addr = ((addr & PAGING_PAGE_MASK) + PAGING_PAGE_SIZE) - sizeof(alloc_block_t);
			uint32_t distance = aligned_addr - addr;

			//does the align adjustment fit in the block?
			if (distance >= size) {
				continue;
			}

			printk_info("find_smallest_hole(): page aligning block @ %x to %x (really starts at %x)", addr, aligned_addr, aligned_addr + sizeof(alloc_block_t));
			//create new block at page aligned addr
			uint32_t new_size = candidate->size - distance - sizeof(alloc_block_t);
			alloc_block_t* aligned = create_block((uint32_t)aligned_addr, new_size);

			insert_block(candidate, aligned);

			//make sure we shrink original candidate since some of it is now in new aligned block
			candidate->size = candidate->size - aligned->size - sizeof(alloc_block_t);
			//all done!

			return aligned;
		}
		return candidate;
	} while ((candidate = candidate->next) != NULL && ((uint32_t)candidate < heap->end_address));

	//didn't find any matches
	printf_err("find_smallest_hole(): no holes big enough (size: 0x%08x align: %d)", size, align);
	panic("heap could not accomodate alloc");
	return NULL;
}

void kheap_init() {
	boot_info_t *info = boot_info_get();

	// Map a big block we'll chop up to use as a heap pool
	uint32_t heap_size = KHEAP_INITIAL_SIZE;
	uint32_t heap_start = vmm_alloc_continuous_range(vmm_active_pdir(), heap_size, true);
	uint32_t heap_end = heap_start + heap_size;
	printf_info("Kernel heap allocated at [0x%08x - 0x%08x]", heap_start, heap_end);

	kheap->start_address = heap_start;
	kheap->end_address = heap_end;
	kheap->supervisor = true;
	kheap->readonly = false;

	//we start off with one large free block
	//this represents the whole heap at this point
	create_block(heap_start, heap_size);

	info->heap_kernel = kheap;
}

//reserve heap block with size >= 'size'
//will page align block if 'align'
void* alloc(uint32_t size, uint8_t align, heap_t* heap) {
	//find smallest hole that will fit
	alloc_block_t* candidate = find_smallest_hole(size, align, heap);

	//handle if we couldn't find a candidate block
	if (!candidate) {
		panic("heap couldn't accommodate alloc, needs growing");
	}

	//check if block should be split into 2 blocks
	//only worth it if the size of the second block will be greater than at least a block header
	if (candidate->size - size > sizeof(alloc_block_t) + MIN_BLOCK_SIZE) {
		//create second block
		uint32_t split_block = (uint32_t)candidate + sizeof(alloc_block_t) + size;
		uint32_t split_size = candidate->size - size - sizeof(alloc_block_t);

		create_block(split_block, split_size);

		//insert new block into linked list
		insert_block(candidate, (alloc_block_t*)split_block);

		if (candidate->next != (alloc_block_t*)split_block || ((alloc_block_t*)split_block)->prev != candidate) {
			printk_err("Heap insertion failed!");
			heap_fail(candidate);
		}

		//shrink block we just split in two
		candidate->size = size;
	}

	//add this allocation to used memory
	used_bytes += size;

	//candidate is now in use
	candidate->free = false;

	//start off by clearing this block
	uint32_t* ptr = (uint32_t*)((uint32_t)candidate + sizeof(alloc_block_t));
	memset(ptr, 0, candidate->size);

	return (void*)((uint32_t)candidate + sizeof(alloc_block_t));
}

//merge two contiguous heap blocks if both are free
//left and right _must_ be immediately adjacent, in that order
bool merge_blocks(alloc_block_t* left, alloc_block_t* right) {
	//make sure both blocks are free
	if (!left->free || !right->free) {
		return false;
	}
	//ensure left is smaller than right
	if (left > right) {
		printk_err("merge_blocks(): left was larger than right!");
		return false;
	}
	//ensure these blocks are adjacent
	if (left->next != right) {
		printk_err("merge_blocks(): left->next was %x, not %x", left->next, right);
		return false;
	}

	//ready to merge
	//increase left block by size of right block and right block's header
	left->size += right->size + sizeof(alloc_block_t);
	//remove right from list
	left->next = right->next;
	left->next->prev = left;

	//all done
	return true;
}

//unreserve heap block which points to p
//also, attempts to re-merge free blocks in heap
void free(void* p, heap_t* UNUSED(heap)) {
	if (p == 0) {
		return;
	}

	//get header associated with this pointer
	alloc_block_t* header = (alloc_block_t*)((uint32_t)p - sizeof(alloc_block_t));

	//ensure these are valid
	if (header->magic != HEAP_MAGIC) {
		printk_err("free() invalid block @ %x", header);
		heap_fail(header);
	}

	//we're about to free this memory, untrack it from used memory
	used_bytes -= header->size;

	//turn this into a hole
	header->free = true;

	//attempt to merge with previous block
	if (header->prev) {
		merge_blocks(header->prev, header);
	}
	//attempt to merge with next block
	if (header->next) {
		merge_blocks(header, header->next);
	}

	//TODO contract if this block is at end of heap space
}

/*
 * Public API wrappers
 * Includes locking
 */

void* kmalloc_int(uint32_t sz, int align, uint32_t* phys) {
	static lock_t _lock = {0};
	lock(&_lock);
	void* ret = _kmalloc_int_unlocked(sz, align, phys);
	unlock(&_lock);
	return ret;
}

void kfree(void* p) {
	static lock_t _lock = {0};
	lock(&_lock);
	_kfree_unlocked(p);
	unlock(&_lock);
}

/*
 * Heap debugging
 */

void heap_fail(void* dump) {
	heap_print(10);
	dump_stack(dump);
	memdebug();

	printk_err("PID %d encountered corrupted heap", getpid());
	panic("corrupted heap");
}

void kheap_debug() {
	boot_info_t* boot = boot_info_get();
	alloc_block_t* b = first_block(boot->heap_kernel);
	printf("------------ heap blocks ------------\n");
	do {
		const char* state = "allc";
		if (b->free) {
			state = "free";
		}
		printf("%s block @ 0x%08x - 0x%08x\n", state, b, b->size);
	} while ((b = b->next) != NULL);
	printf("-------------------------------------\n");
}

//prints last 'display_count' alloc's in heap
void kheap_print(heap_t* heap, int display_count) {
	alloc_block_t* counter = first_block(heap);
	int block_count = 0;
	while (counter) {
		block_count++;
		counter = counter->next;
	}
	int starting_idx = 0;
	if (block_count > display_count && display_count != -1) {
		starting_idx = block_count - display_count;
	}

	//advance to starting_idx
	alloc_block_t* curr = first_block(heap);
	for (int i = 0; i < starting_idx; i++) {
		curr = curr->next;
	}

	printk("|-------------------------------------|\n");
	printk("| Heap state (%d more heap items)    |\n", starting_idx);
	printk("|------------|------------|-----------|\n");
	printk("| addr       | size       | free      |\n");
	printk("|------------|------------|-----------|\n");
	while (curr) {
		printk("| %x | %x | %s      %s|\n", (uint32_t)curr, curr->size, (curr->free) ? "free" : "used", (curr->magic == HEAP_MAGIC) ? "" : "invalid header");
		curr = curr->next;
	}
	printk("|-------------------------------------|\n");
}

void heap_print(int count) {
	kheap_print(kheap, count);
}

void heap_verify_integrity() {
	//check heap integrity
	alloc_block_t* tmp = first_block(kheap);
	//search every hole
	do {
		if (tmp->magic != HEAP_MAGIC) {
			printk_err("alloc() self check: block @ %x had invalid magic", (uint32_t)tmp);
			heap_fail(tmp);
		}
	} while ((tmp = tmp->next) != NULL);
}

#define MAX_FILES 256
#define MAX_FILENAME 64
//array of filenames using kmalloc
static char kmalloc_users[MAX_FILES][MAX_FILENAME];
//array of bytes used by each file corresponding to kmalloc_users
static int kmalloc_users_used[MAX_FILES];
//keep track of current amount of users in array
static int current_filecount = 0;
void kmalloc_track_int(char* file, int UNUSED(line), uint32_t size) {
	//if this is first run, memset arrays
	if (!current_filecount) {
		memset(kmalloc_users, 0, sizeof(kmalloc_users));
		memset(kmalloc_users_used, 0, sizeof(kmalloc_users_used));
	}
	//are we about to exceed array bounds?
	if (current_filecount + 1 >= MAX_FILES) {
		printk("kmalloc_track_int() exceeds array\n");
		while (1) {}
	}

	//search if this file is already in users
	char* line = 0;
	int idx = -1;
	for (int i = 0; i < current_filecount; i++) {
		line = (char*)kmalloc_users[i];

		//is this a match?
		if (strcmp(file, line) == 0) {
			idx = i;
			break;
		}
	}

	//did this user already exist?
	if (idx == -1) {
		idx = current_filecount;
		strcpy(kmalloc_users[current_filecount++], file);
	}
	kmalloc_users_used[idx] += size;
}

void memdebug() {
	//find length of longest filename
	int longest_len = 0;
	for (int i = 0; i < current_filecount; i++) {
		int curr_len = strlen(kmalloc_users[i]);
		longest_len = MAX(longest_len, curr_len);
	}

	printk("\n---memdebug---\n");
	for (int i = 0; i < current_filecount; i++) {
		//print filename
		printk("%s:", kmalloc_users[i]);

		//print out some spaces
		//# of spaces is the difference between this filename's length and the
		//longest filename's length
		//this is so the output is aligned in syslog
		int diff = longest_len - strlen(kmalloc_users[i]);
		for (int j = 0; j < diff; j++) {
			printk(" ");
		}

		printk(" %x bytes\n", kmalloc_users_used[i]);
	}
	printk("--------------\n");
}

uint32_t used_mem() {
	return used_bytes;
}
