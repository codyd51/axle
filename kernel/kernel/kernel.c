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
#include "ap_bootstrap.h"

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
    boot_info_t* boot_info = boot_info_get();
    // Copy the AP bootstrap from wherever it was loaded into physical memory into its bespoke location
    // This location matches where the compiled code expects to be loaded.
    // AP startup code must also be placed below 1MB, as APs start up in real mode.
    printf("Copy AP bootstrap from [0x%p - 0x%p] to [0x%p - 0x%p]\n",
           boot_info->ap_bootstrap_base,
           boot_info->ap_bootstrap_base + boot_info->ap_bootstrap_size,
           AP_BOOTSTRAP_CODE_PAGE,
           AP_BOOTSTRAP_CODE_PAGE + boot_info->ap_bootstrap_size
    );
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_CODE_PAGE), (void*)PMA_TO_VMA(boot_info->ap_bootstrap_base), boot_info->ap_bootstrap_size);

    // Set up the protected mode GDT parameter
    uintptr_t gdt_size = 0;
    gdt_descriptor_t* protected_mode_gdt = gdt_create_for_protected_mode(&gdt_size);
    // Ensure the GDT fits in the expected size
    assert(sizeof(gdt_pointer_t) + gdt_size <= AP_BOOTSTRAP_PARAM_OFFSET_LONG_MODE_GDT, "Protected mode GDT was too big to fit in its parameter slot");
    printf("Got protected mode gdt %p\n", protected_mode_gdt);

    gdt_pointer_t protected_mode_gdt_ptr = {0};
    uint32_t relocated_protected_mode_gdt_addr = AP_BOOTSTRAP_PARAM_PROTECTED_MODE_GDT + 8;
    protected_mode_gdt_ptr.table_base = (uintptr_t)relocated_protected_mode_gdt_addr;
    protected_mode_gdt_ptr.table_size = gdt_size;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PROTECTED_MODE_GDT), &protected_mode_gdt_ptr, sizeof(gdt_pointer_t));
    memcpy((void*)PMA_TO_VMA(relocated_protected_mode_gdt_addr), protected_mode_gdt, gdt_size);
    // Copied the Protected Mode GDT to the data page, we can free it now
    kfree(protected_mode_gdt);

    // Set up the long mode GDT parameter
    // Re-use the same GDT the BSP is using
    gdt_pointer_t* current_long_mode_gdt = kernel_gdt_pointer();
    printf("Got long mode GDT %p, table size %p\n", current_long_mode_gdt, current_long_mode_gdt->table_size);
    gdt_descriptor_t* long_mode_gdt_entries = (gdt_descriptor_t*)current_long_mode_gdt->table_base;
    // We need to create a new pointer as the existing one points to high memory
    gdt_pointer_t long_mode_gdt_ptr = {0};
    uint32_t relocated_long_mode_gdt_addr = AP_BOOTSTRAP_PARAM_LONG_MODE_GDT + 8;
    long_mode_gdt_ptr.table_base = (uintptr_t)relocated_long_mode_gdt_addr;
    long_mode_gdt_ptr.table_size = current_long_mode_gdt->table_size;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_GDT), &long_mode_gdt_ptr, sizeof(gdt_pointer_t));
    memcpy((void*)PMA_TO_VMA(relocated_long_mode_gdt_addr), (void*)current_long_mode_gdt->table_base, current_long_mode_gdt->table_size);

    // Set up a virtual address space
    pml4e_t* bsp_pml4 = (pml4e_t*)PMA_TO_VMA(boot_info->vas_kernel->pml4_phys);
    uint64_t ap_pml4_phys_addr = pmm_alloc();
    pml4e_t* ap_pml4 = (pml4e_t*)PMA_TO_VMA(ap_pml4_phys_addr);
    // Copy all memory mappings from the BSP virtual address space
    for (int i = 0; i < 512; i++) {
        printf("Set %p[%d] = %p[%d]\n", ap_pml4, i, bsp_pml4,i);
        ap_pml4[i] = bsp_pml4[i];
    }
    // Identity map the pages where the AP bootstrap is running
    //// 2 pages: 1 for the code page, 1 for the data page
    //_map_region_4k_pages(ap_pml4, AP_BOOTSTRAP_CODE_PAGE, PAGE_SIZE * 2, AP_BOOTSTRAP_CODE_PAGE, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
    // Identity map the low 4G
    // TODO(PT): This will cause problems if any of the paging structures are allocated above 4GB...
    void _map_region_4k_pages(pml4e_t* page_mapping_level4_virt, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start, vas_range_access_type_t access_type, vas_range_privilege_level_t privilege_level);
    _map_region_4k_pages(ap_pml4, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
    // Copy the PML4 pointer
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PML4), &ap_pml4_phys_addr, sizeof(ap_pml4_phys_addr));

    printf("Bootloader provided RSDP 0x%x\n", boot_info->acpi_rsdp);

    // Parse the ACPI tables and start up the other APs
    acpi_parse_root_system_description(boot_info->acpi_rsdp);
    asm("cli");
    while (1) {}

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
