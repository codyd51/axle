#include "surface.h"
#include <kernel/util/shmem/shmem.h>
#include <kernel/util/ipc/ipc.h>
#include <kernel/multitasking/tasks/task.h>

Surface* surface_make(uint32_t width, uint32_t height, uint32_t dest_pid) {
	task_t* dest = task_with_pid(dest_pid);
	ASSERT(dest, "surface_create invalid PID %d", dest_pid);

	uint32_t bytes_needed = width * height * gfx_bpp();
	printf("surface_make(%d, %d) bytes needed %x\n", width, height, bytes_needed);

	char* kernel_base = NULL;
	char* base = shmem_get_region_and_map(dest->page_dir, bytes_needed, 0x0, &kernel_base, true);

	Surface* surface_to_send = kmalloc(sizeof(Surface));
	surface_to_send->base_address = (uint8_t*)base;
	surface_to_send->size = bytes_needed;
	surface_to_send->width = width;
	surface_to_send->height = height;
	surface_to_send->bpp = gfx_bpp();
	surface_to_send->kernel_base = (uint8_t*)kernel_base;

	Surface* ipc_destination;
	ipc_send((char*)surface_to_send, sizeof(Surface), 4, (char**)&ipc_destination);
	printk_info("IPC placed OUR NEW SURFACE at %x base_address %x kern %x", surface_to_send, surface_to_send->base_address, surface_to_send->kernel_base);

	printk_info("ipc_destination->width %d ipc_destination->height %d", ipc_destination->width, ipc_destination->height);

	Screen* s = gfx_screen();
	array_m_insert(s->surfaces, ipc_destination);

	return surface_to_send;
}
