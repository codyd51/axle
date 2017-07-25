#include "ipc.h"
#include <kernel/util/shmem/shmem.h>
#include <kernel/util/multitasking/tasks/task.h>

static void expand_ipc_region(task_t* dest, uint32_t size) {
	//is there any ipc region yet?
	if (!dest->ipc_state) {
		dest->ipc_state = kmalloc(sizeof(ipc_state_t));
		ipc_state_t* ipc_region = dest->ipc_state;
		//create initial ipc region
		//find unused region we can place ipc mapping
		char* shmem_region = shmem_get(dest->page_dir, size, 0x0, &ipc_region->kernel_addr, false);
		ipc_region->region_start = shmem_region;
		ASSERT(!((uint32_t)ipc_region->region_start % PAGE_SIZE), "ipc region start wasn't page aligned");

		uint32_t page_padded_size = size;
		if (page_padded_size % PAGE_SIZE) {
			uint32_t overlap = page_padded_size % PAGE_SIZE;
			page_padded_size += PAGE_SIZE - overlap;
		}
		
		ipc_region->region_end = ipc_region->region_start + page_padded_size;
		ASSERT(!((uint32_t)ipc_region->region_end % PAGE_SIZE), "ipc region end wasn't page aligned");

		ipc_region->next_unused_addr = ipc_region->region_start;
		return;
	}
	ASSERT(0, "expanding an IPC region after creation not supported");
}

int ipc_send(char* data, uint32_t size, uint32_t dest_pid, char** destination) {
	task_t* dest = task_with_pid(dest_pid);
	if (!dest->ipc_state) {
		expand_ipc_region(dest, PAGE_SIZE * 4);
	}

	ipc_state_t* ipc_region = dest->ipc_state;
	uint32_t available_space = (uint32_t)ipc_region->region_end - (uint32_t)ipc_region->next_unused_addr;
	ASSERT(size <= available_space, "ipc_send tried to send message larger than capacity");

	*destination = ipc_region->next_unused_addr;
	memcpy(ipc_region->kernel_addr, data, size);
	ipc_region->next_unused_addr += size;
	return 0;
}

