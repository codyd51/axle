#include "shmem.h"
#include <std/std.h>
#include <kernel/multitasking//tasks/task.h>

#define PAGE_MASK 0xFFFFF000

char* find_unmapped_region(page_directory_t* dir, uint32_t size, uint32_t begin_searching_at) {
	if (!dir || !size) {
		return NULL;
	}

	//uint32_t run_start, run_end;
	//bool in_run = false;
	//1024 page tables in page dir
	for (int i = 0; i < 1024; i++) {
		page_table_t* tab = dir->tables[i];

		//page tables map 1024 4kb pages
		//page directories contains 1024 page tables
		//therefore, each page table maps 4mb of the virtual addr space
		//for a given table index and page index, the virtual address is:
		//table index * (range mapped by each table) + page index * (range mapped by each page)
		//table index * 4mb + page index * 4kb
		uint32_t page_table_virt_range = PAGE_SIZE * 1024;
		uint32_t current_tab_virt_start = (i * page_table_virt_range);

		//are we at a smaller address than client wants?
		if (current_tab_virt_start < begin_searching_at) {
			continue;
		}

		if (!tab) {
			//found unused page table!
			//we can alloc this table and alloc requested mem
			//a quick and dirty way to alloc a page table is to call get_page on a page
			//in the table, with @make = 1
			get_page(current_tab_virt_start, 1, dir);
			page_table_t* tab = dir->tables[i];
			ASSERT(tab, "find_unmapped_region() tab still null after get_page");
			//we should be able to use this page table AS LONG AS @p size is less than 4mb, the space mapped by a single page table
			//TODO handle shared mem larger than a page table
			if (page_table_virt_range >= size) {
				return (char*)current_tab_virt_start;
			}
		}

		/*
		for (int j = 0; j < 1024; j++) {
			uint32_t current_virt_addr = (i * page_table_virt_range) + (j * PAGE_SIZE);
			//are we at a smaller address than client wants?
			if (current_virt_addr < begin_searching_at) {
				continue;
			}

			if (!tab->pages[j].present && !tab->pages[j].frame) {
				//page not present
				//start run if we're not in one
				if (!in_run) {
					in_run = true;
					run_start = (i * page_table_virt_range) + (j * PAGE_SIZE);
				}
			}
			else {
				//are we in a run?
				if (in_run) {
					//run finished!
					//run ends on previous page
					run_end = (i * page_table_virt_range) + ((j - 1) * PAGE_SIZE);
					printf("[%x - %x]\n", run_start, run_end);

					in_run = false;

					//did we find a run of sufficient size?
					if (run_end - run_start >= size) {
						//found unmapped region of desired size!
						char* ptr = (char*)run_start;
						return ptr;
					}
				}
			}
		}
		*/
	}
	ASSERT(0, "find_unmapped_region failed");
	return NULL;
}

char* shmem_create_map(page_directory_t* dir, uint8_t* backing_memory, uint32_t size, uint32_t begin_searching_at, bool writeable) {
	printk_info("shmem_create_map dir %x backing_memory %x size %x", dir, backing_memory, size);

	if ((uint32_t)backing_memory & ~PAGE_MASK) {
		printf_err("shmem_create_map() backing_memory wasn't page aligned!");
		return NULL;
	}

	char* unmapped_region = find_unmapped_region(dir, size, begin_searching_at);
	printk_info("shmem_create_map making map at %x in dir %x", unmapped_region, dir);

	uint32_t padded = size;
	if (padded & ~PAGE_MASK) {
		padded = (padded + PAGE_SIZE) & PAGE_MASK;
	}

	//now, map new memory into client page dir
	//no need to account for overlap because padded should be page aligned
	uint32_t page_count = padded / PAGE_SIZE;
	for (uint32_t i = 0; i < page_count; i++) {
		char* local_to_map = (char*)(backing_memory + (i * PAGE_SIZE));
		page_t* local_page = get_page((uint32_t)local_to_map, 1, page_dir_kern());
		ASSERT(local_page, "shmem_get couldn't get local page");

		char* map_destination = (char*)(unmapped_region + (i * PAGE_SIZE));
		page_t* client_page = get_page((uint32_t)map_destination, 1, dir);
		ASSERT(client_page, "shmem_get couldn't get destination page");
		ASSERT(!client_page->frame, "shmem_get client page already assigned!");

		memset(local_to_map, 0, PAGE_SIZE);
		client_page->present = 1;
		client_page->rw = writeable;
		client_page->user = 1;
		client_page->frame = local_page->frame;
		invlpg(map_destination);
		invlpg(local_to_map);
	}

	printf_info("shmem mapped %x in kernel to %x in client", backing_memory, unmapped_region);

	return unmapped_region;
}

char* shmem_get_region_and_map(page_directory_t* dir, uint32_t size, uint32_t begin_searching_at, char** kernel_mapped_address, bool writeable) {
	//alloc memory for region
	//make sure the block starts on a page aligned address
	//also, pad @p size until it's page aligned
	//this is so pages mapped to user only contain memory dedicated to user use, 
	//and there is no heap data contained in them.
	uint32_t padded = size;
	if (padded & ~PAGE_MASK) {
		padded = (padded + PAGE_SIZE) & PAGE_MASK;
	}
	printk("shmem_get_region_and_map padded size %d", padded);

	uint8_t* backing_memory = kmalloc_a(padded);
	if (kernel_mapped_address) {
		*kernel_mapped_address = (char*)backing_memory;
	}

	return shmem_create_map(dir, backing_memory, padded, begin_searching_at, writeable);
}

