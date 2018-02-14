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

extern uint32_t _kernel_image_end;
uint32_t placement_address = (uint32_t)&_kernel_image_end;

extern page_directory_t* kernel_directory;
extern page_directory_t* current_directory;

heap_t kheap_raw = {0};
heap_t* kheap = &kheap_raw;
lock_t* mutex = 0;
static uint32_t used_bytes;

//root kmalloc function
//increments placement_address if there is no heap
//otherwise, pass through to heap with given options
void* kmalloc_int(uint32_t sz, int align, uint32_t* phys) {
	//if the heap already exists, pass through
	if (kheap) {
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

void kfree(void* p) {
	uint32_t addr = (uint32_t)p;
	if (addr <= kheap->start_address || addr >= kheap->end_address) {
		printk("kfree() invalid block %x\n", addr);
	}
	free(p, kheap);
}

void heap_fail(void* dump) {
	heap_print(10);
	dump_stack(dump);
	memdebug();

	printk_err("PID %d encountered corrupted heap. Killing task...", getpid());
	_kill();
}

//create a heap header at addr, where the block in questoin is size bytes
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

	lock(mutex);

	if (prev->next) {
		new->next = prev->next;
	}
	if (new->next) {
		new->next->prev = new;
	}

	prev->next = new;
	new->prev = prev;

	unlock(mutex);
}

//get the first block header in linked list
static alloc_block_t* first_block(heap_t* heap) {
	return (alloc_block_t*)heap->start_address;
}

//find the smallest block at least size bytes big, and,
//if page aligning is requested, is large enough to be page aligned
//(if so, page-aligns block and returns aligned block)
static alloc_block_t* find_smallest_hole(uint32_t size, bool align, heap_t* heap) {
	//printk_info("find_smallest_hole(): %x bytes align? %d", size, align);
	//start off with first block
	alloc_block_t* candidate = first_block(heap);

	//lock(mutex);

	//search every hole
	do {
		ASSERT(candidate->magic == HEAP_MAGIC, "find_smallest_hole() detected heap corruption");
		if (candidate->free) {
			if (candidate->size >= size) {
				//found valid header!
				//printk_info("find_smallest_hole() found likely candidate %x", (uint32_t)candidate);

				//attempt to align if user requested
				//make sure addr isn't already page aligned before aligning
				uint32_t addr = (uint32_t)candidate + sizeof(alloc_block_t);
				if (align && (addr & 0xFFF)) {
					//find distance to page align
					uint32_t aligned_addr = ((addr & 0xFFFFF000) + PAGING_PAGE_SIZE) - sizeof(alloc_block_t);
					uint32_t distance = aligned_addr - addr;

					//does the align adjustment fit in the block?
					if (distance < size) {
//#ifdef DEBUG
						printk_info("find_smallest_hole(): page aligning block @ %x to %x (really starts at %x)", addr, aligned_addr, aligned_addr + sizeof(alloc_block_t));
//#endif

						//create new block at page aligned addr
						uint32_t new_size = candidate->size - distance - sizeof(alloc_block_t);
						alloc_block_t* aligned = create_block((uint32_t)aligned_addr, new_size);

						insert_block(candidate, aligned);

						//make sure we shrink original candidate since some of it is now in new aligned block
						candidate->size = candidate->size - aligned->size - sizeof(alloc_block_t);
						//all done!

						unlock(mutex);
						return aligned;
					}
					else {
						continue;
					}
				}
				return candidate;
			}
		}
	} while ((candidate = candidate->next) != NULL && ((uint32_t)candidate < heap->end_address));

	//unlock(mutex);

	//didn't find any matches
	printk_err("find_smallest_hole(): found no holes large enough (size: %x align: %d)", size, align);
	return NULL;
}

void kheap_init() {
    boot_info_t* info = boot_info_get();
    uint32_t start = info->kernel_image_end + 0x100000;
    uint32_t end_addr = start + KHEAP_INITIAL_SIZE;
    uint32_t max = end_addr;
    uint32_t max_size = max - start;

	//start and end MUST be page aligned
	ASSERT(start % PAGING_PAGE_SIZE == 0, "start wasn't page aligned");
	ASSERT(end_addr % PAGING_PAGE_SIZE == 0, "end_addr wasn't page aligned");

    printf_info("Creating kernel heap at [0x%08x - 0x%08x]", start, end_addr);

	//write start, end, and max addresses into heap structure
	kheap->start_address = start;
	kheap->end_address = end_addr;
	kheap->max_address = max;
	kheap->supervisor = true;
	kheap->readonly = false;

    //map this memory into kernel page directory
    page_directory_t* kernel_vmm = info->vmm_kernel;
    vmm_map_region(kernel_vmm, start, KHEAP_INITIAL_SIZE);

	//we start off with one large free block
	//this represents the whole heap at this point
	create_block(start, end_addr - start);

    info->heap_kernel = kheap;
}

void expand(uint32_t UNUSED(new_size), heap_t* UNUSED(heap)) {
	//TODO code this
}

uint32_t contract(int32_t UNUSED(new_size), heap_t* UNUSED(heap)) {
	//TODO code this
	return -1;
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
			kernel_begin_critical();
			while (1) {}
		}
	} while ((tmp = tmp->next) != NULL);
}

static void heap_expand(heap_t* heap, uint32_t expand_size) {
	lock(mutex);

	uint32_t curr_size = heap->end_address - heap->start_address;
	if (curr_size + expand_size >= heap->max_address) {
		ASSERT(0, "Heap ran out of space!\n");
		while (1) {}
	}

	alloc_block_t* curr = first_block(heap);
	while (curr->next) {
		curr = curr->next;
	}
	if (!curr->free) {
		uint32_t new_block_addr = (uint32_t)curr + sizeof(alloc_block_t) + curr->size;
		printk("heap_expand() creating new block at end of heap of size %x\n", new_block_addr);

		alloc_block_t* new_block = create_block(new_block_addr, expand_size);
		insert_block(curr, new_block);

		heap->end_address += sizeof(alloc_block_t) + expand_size;
	}
	else {
		printk("heap_expand() expanding block %x from %x to %x\n", curr, curr->size, curr->size + expand_size);
		curr->size += expand_size;
		heap->end_address += expand_size;
	}

	unlock(mutex);
}

//reserve heap block with size >= 'size'
//will page align block if 'align'
void* alloc(uint32_t size, uint8_t align, heap_t* heap) {

	//find smallest hole that will fit
	alloc_block_t* candidate = find_smallest_hole(size, align, heap);

	//handle if we couldn't find a candidate block
	if (!candidate) {
		//expand heap
		//TODO fill in
		//ASSERT(0, "alloc() %x bytes failed, find_smallest_hole() had no candidates\n");
		uint32_t curr_size = heap->end_address - heap->start_address;
		//uint32_t expand_size = curr_size + size;
		uint32_t expand_size = curr_size * 2;
		expand_size = MIN(expand_size, heap->max_address - heap->start_address);
		printk("Heap could not fit alloc %x, expanding size by %x\n", size, expand_size);
		heap_expand(heap, expand_size);
		//heap should now have enough space, try alloc again
		return alloc(size, align, heap);
	}

#ifdef DEBUG
	printk("DEB M %x %x\n", candidate, candidate->size);
#endif

	lock(mutex);

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
			while (1) {}
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

	unlock(mutex);

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

	lock(mutex);

	//ready to merge
	//increase left block by size of right block and right block's header
	left->size += right->size + sizeof(alloc_block_t);
	//remove right from list
	left->next = right->next;
	left->next->prev = left;

	//all done
	unlock(mutex);

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
	//ASSERT(header->magic == HEAP_MAGIC, "invalid header magic in %x (got %x)", p, header->magic);
	if (header->magic != HEAP_MAGIC) {
		printk_err("free() invalid block @ %x", header);
		heap_fail(header);
		while (1) {}
	}

#ifdef DEBUG
	printk("DEB F %x %x\n", p, header->size);
#endif

	lock(mutex);

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

	unlock(mutex);
	//TODO contract if this block is at end of heap space
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
