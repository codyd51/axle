#include "ipc.h"
#include <kernel/util/shmem/shmem.h>
#include <kernel/multitasking/tasks/task.h>

#define PAGE_MASK 0xFFFFF000

static void expand_ipc_region(task_t* dest, uint32_t size) {
	//is there any ipc region yet?
	if (!dest->ipc_state) {
		printf("initial ipc map pid %d\n", dest->id);
		dest->ipc_state = kmalloc(sizeof(ipc_state_t));
		ipc_state_t* ipc_region = dest->ipc_state;
		memset(ipc_region, 0, sizeof(ipc_region));

		//create initial ipc region
		//find unused region we can place ipc mapping
		uint32_t page_padded_size = size;
		if (page_padded_size & ~PAGE_MASK) {
			page_padded_size = (page_padded_size + PAGE_SIZE) & PAGE_MASK;
		}

		char* shmem_region = shmem_get_region_and_map(dest->page_dir, page_padded_size, 0x0, &ipc_region->kernel_addr, true);
		ipc_region->region_start = shmem_region;
		ASSERT(!((uint32_t)ipc_region->region_start % PAGE_SIZE), "ipc region start wasn't page aligned");

		ipc_region->region_end = ipc_region->region_start + page_padded_size;
		ASSERT(!((uint32_t)ipc_region->region_end % PAGE_SIZE), "ipc region end wasn't page aligned");

		ipc_region->next_unused_addr = ipc_region->region_start;
		return;
	}
	ASSERT(0, "expanding an IPC region after creation not supported");
}

int ipc_send(char* data, uint32_t size, uint32_t dest_pid, char** destination) {
	if (!destination || !data || !size) {
		printf_err("ipc_send() invalid args");
		return -1;
	}

	task_t* dest = task_with_pid(dest_pid);
	if (!dest->ipc_state) {
		printk_info("expanding PID %d ipc region..", dest_pid);
		expand_ipc_region(dest, PAGE_SIZE * 4);
	}
	printk_info("finished expadning");

	ipc_state_t* ipc_region = dest->ipc_state;
	uint32_t available_space = (uint32_t)ipc_region->region_end - (uint32_t)ipc_region->next_unused_addr;
	ASSERT(size <= available_space, "ipc_send tried to send message larger than capacity");

	*destination = ipc_region->next_unused_addr;
	uint32_t used_bytes = (uint32_t)ipc_region->next_unused_addr - (uint32_t)ipc_region->region_start;
	memcpy(ipc_region->kernel_addr + used_bytes, data, size);
	ipc_region->next_unused_addr += size;
	printk_info("*destination %x used_bytes %x next_unused_addr %x", *destination, used_bytes, ipc_region->next_unused_addr);
	return 0;
}
