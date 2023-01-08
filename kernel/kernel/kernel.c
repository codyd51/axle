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

#include "kernel.h"
#include "kernel/segmentation/gdt_structures.h"

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
    draw(boot_info, 0x0000ffff);
    //draw_string_oneshot("Parsing boot info...\n");
    //while (1) {}
    // Environment info
    boot_info_read(boot_info);
    //draw(boot_info, 0x0000ff00);
    //boot_info_dump();
    //draw_string_oneshot("Got boot info\n");

    // Descriptor tables
    gdt_init();
    draw(boot_info, 0x000000ff);
    interrupt_init();
    draw(boot_info, 0xff00ff);
    //draw_string_oneshot("Enabled interrupts");
    //draw_string_oneshot("Will next try PIT");

    // PIT and serial drivers
    pit_timer_init(PIT_TICK_GRANULARITY_1MS);
    //draw_string_oneshot("Enabled PIT");
    //draw(boot_info, 0x444444);
    serial_init();
    //draw_string_oneshot("Enabled serial");
    //draw(boot_info, 0xffff00);

    // Kernel features
    pmm_init();
    //draw(boot_info, 0xffffff);
    pmm_dump();
    //draw(boot_info, 0xff45ff);
    vmm_init(boot_info->boot_pml4);

    //draw(boot_info, 0x00ff0000);
    //draw_string_oneshot_ex("Enabling tasking...", false);

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
    boot_info_t* info = boot_info_get();
    // Copy the AP bootstrap from wherever it was loaded into physical memory into its bespoke location
    // This location matches where the compiled code expects to be loaded.
    // AP startup code must also be placed below 1MB, as APs start up in real mode.
    printf("Copy AP bootstrap from [0x%p - 0x%p] to [0x%p - 0x%p]\n",
           info->ap_bootstrap_base,
           info->ap_bootstrap_base + info->ap_bootstrap_size,
           AP_BOOTSTRAP_CODE_PAGE,
           AP_BOOTSTRAP_CODE_PAGE + info->ap_bootstrap_size
    );
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PAGE), (void*)PMA_TO_VMA(info->ap_bootstrap_base), info->ap_bootstrap_size);
    uint32_t param1 = 0xdeadbeef;
    uint32_t param2 = 0xcafebabe;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PAGE + 8), &param1, sizeof(uint32_t));
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PAGE + 12), &param2, sizeof(uint32_t));

    printf("Bootloader provided RSDP 0x%x\n", info->acpi_rsdp);

    // Parse the ACPI tables and start up the other APs
    acpi_parse_root_system_description(info->acpi_rsdp);
    // This must happen before the second half of the bootstrap, as the ACPI tables are in the low-memory identity map.
    // The low-memory identity map is trashed once we enter the second half.
    // (See also: comment in bootloader/paging.c)

    //draw_string_oneshot("Bootstrap part 2");
    //draw(boot_info_get(), 0x00ff00ff);
    // We're now fully set up in high memory
    syscall_init();

    //draw_string_oneshot("Syscalls done");

    // Initialize PS/2 controller
    // (and sub-drivers, such as a PS/2 keyboard and mouse)
    ps2_controller_init();
    //draw_string_oneshot("PS/2 done");

    // Early boot is finished
    // Multitasking and program loading is now available
    // 
    // Launch the file server, which will load the ramdisk and launch all the 
    // specified startup programs
    task_spawn__with_args("launch_fs_server", FS_SERVER_EXEC_TRAMPOLINE_NAME, 0, 0, 0);

    //draw_string_oneshot("Bootstrap done");

    // Bootstrapping complete - kill this process
    printf("[t = %d] Bootstrap task [PID %d] will exit\n", time(), getpid());
    task_die(0);
    assert(0, "task_die should have stopped execution");
}
