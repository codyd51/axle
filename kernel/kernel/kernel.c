#include <stddef.h>
#include <stdint.h>

#include <kernel/boot_info.h>

// Arch-specific
#include <kernel/segmentation/gdt.h>

// Memory
#include <kernel/pmm/pmm.h>
#include <kernel/vmm/vmm.h>

// Peripherals interaction
#include <kernel/drivers/kb/kb.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/mouse/mouse.h>
#include <kernel/drivers/serial/serial.h>

// Kernel features
#include <kernel/syscall/syscall.h>
#include <kernel/util/elf/elf.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/smp.h>

#include "kernel.h"

static void _kernel_bootstrap_part2(void);

void FS_SERVER_EXEC_TRAMPOLINE_NAME(uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    boot_info_t* boot_info = boot_info_get();
    const char* program_name = "fs_server";
    char* argv[] = {program_name, NULL};
    elf_load_buffer(program_name, argv, PMA_TO_VMA(boot_info->file_server_elf_start), boot_info->file_server_elf_size, false);
}

void draw(axle_boot_info_t* bi, int color) {
    return;
	uint32_t* base = (uint32_t*)bi->framebuffer_base;
	for (uint32_t y = 0; y < bi->framebuffer_height; y++) {
		for (uint32_t x = 0; x < bi->framebuffer_width; x++) {
			base[y * bi->framebuffer_width + x] = color;
		}
	}
	//wait();
}

void draw2(int color) {
    return;
    boot_info_t* b = boot_info_get();
    framebuffer_info_t fb = b->framebuffer;
    uint32_t* base = (uint32_t*)fb.address;
	for (uint32_t y = 0; y < fb.height; y++) {
		for (uint32_t x = 0; x < fb.width; x++) {
			base[(y * fb.width) + x] = color;
		}
	}
	//wait();
}

#include <gfx/font/font.h>


void _start(axle_boot_info_t* boot_info) {
    // Environment info
    boot_info_read(boot_info);

    // Descriptor tables
    gdt_init();
    interrupt_init();

    // PIT and serial drivers
    pit_timer_init(PIT_TICK_GRANULARITY_1MS);
    serial_init();

    // Kernel features
    pmm_init();
    pmm_dump();
    vmm_init(boot_info->boot_pml4);

    //draw(boot_info, 0x00ff0000);
    //draw_string_oneshot_ex("Enabling tasking...", false);

    syscall_init();

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();

    // Map the BSP's per-core data structure
    // This needs to be set up early as this data is queried from various pieces of the kernel that are invoked later
    smp_map_bsp_private_info();

    // Higher-level features like multitasking
    // Note that as soon as we enter part 2, we won't be able to read any low memory.
    // This is because the bootloader identity maps low memory, and this identity mapping is trashed once we switch
    // to the kernel's paging structures.
    // (See also: comment in bootloader/paging.c)
    tasking_init(&_kernel_bootstrap_part2);
    // The above call should never return
    assert(false, "Control should have been transferred to a new stack");
}

static void _kernel_bootstrap_part2(void) {
    // We're now fully set up in high memory

    // Detect and boot other APs
    smp_init();
    smp_core_continue();

    // Early boot is finished
    // Multitasking and program loading is now available
    // 
    // Launch the file server, which will load the ramdisk and launch all the 
    // specified startup programs
    task_spawn__with_args("launch_fs_server", FS_SERVER_EXEC_TRAMPOLINE_NAME, 0, 0, 0);

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
