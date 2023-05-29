#include <stddef.h>
#include <stdint.h>

#include <std/string.h>
#include <std/memory.h>
#include <std/kheap.h>

#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/pmm/pmm.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/segmentation/gdt_structures.h>
#include <kernel/multitasking/tasks/task_small.h>

#include <kernel/ap_bootstrap.h>

#include "smp.h"
#include "stdio.h"

static bool _mapped_cpu_info_in_bsp = false;

void ap_entry_part2(void);

void ap_c_entry(void) {
    tasking_ap_startup(smp_core_continue);
    // Should never return
    assert(false, "tasking_ap_startup was not supposed to return control here");
}

void smp_init(void) {
    boot_info_t* boot_info = boot_info_get();

    // Copy the AP bootstrap from wherever it was loaded into physical memory into its bespoke location
    // This location matches where the compiled code expects to be loaded.
    // AP startup code must also be placed below 1MB, as APs start up in real mode.
    //
    // First, verify our assumption that the AP bootstrap fits into a page
    assert(boot_info->ap_bootstrap_size < PAGE_SIZE, "AP bootstrap was larger than a page!");

    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_CODE_PAGE), (void*)PMA_TO_VMA(boot_info->ap_bootstrap_base), boot_info->ap_bootstrap_size);

    // Set up the protected mode GDT parameter
    uintptr_t gdt_size = 0;
    gdt_descriptor_t* protected_mode_gdt = gdt_create_for_protected_mode(&gdt_size);
    // Ensure the GDT fits in the expected size
    assert(sizeof(gdt_pointer_t) + gdt_size <= AP_BOOTSTRAP_PARAM_OFFSET_LONG_MODE_LOW_MEMORY_GDT, "Protected mode GDT was too big to fit in its parameter slot");

    gdt_pointer_t protected_mode_gdt_ptr = {0};
    uint32_t relocated_protected_mode_gdt_addr = AP_BOOTSTRAP_PARAM_PROTECTED_MODE_GDT + 8;
    protected_mode_gdt_ptr.table_base = (uintptr_t)relocated_protected_mode_gdt_addr;
    protected_mode_gdt_ptr.table_size = gdt_size;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PROTECTED_MODE_GDT), &protected_mode_gdt_ptr, sizeof(gdt_pointer_t));
    memcpy((void*)PMA_TO_VMA(relocated_protected_mode_gdt_addr), protected_mode_gdt, gdt_size);
    // Copied the Protected Mode GDT to the data page, we can free it now
    kfree(protected_mode_gdt);

    // Copy the IDT pointer
    idt_pointer_t* current_idt = kernel_idt_pointer();
    // It's fine to copy the high-memory IDT as the bootstrap will enable paging before loading it
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_IDT), current_idt, sizeof(idt_pointer_t) + current_idt->table_size);

    // Copy the C entry point
    uintptr_t ap_c_entry_point_addr = (uintptr_t)&ap_c_entry;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_C_ENTRY), &ap_c_entry_point_addr, sizeof(ap_c_entry_point_addr));

    // Parse the ACPI tables and store the SMP info in the global system state
    smp_info_t* smp_info = acpi_parse_root_system_description(boot_info->acpi_rsdp);
    boot_info->smp_info = smp_info;

    // Set up the BSP's APIC and the IO APIC
    apic_init(boot_info->smp_info);

    // Set up the BSP's per-core data structure
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        processor_info_t *processor_info = &smp_info->processors[i];
        if (processor_info->apic_id != smp_get_current_core_apic_id(smp_info)) {
            continue;
        }

        // Found the BSP's processor info!
        cpu_core_private_info_t* cpu_core_info = cpu_private_info();
        cpu_core_info->processor_id = processor_info->processor_id;
        cpu_core_info->apic_id = processor_info->apic_id;
        cpu_core_info->local_apic_phys_addr = smp_info->local_apic_phys_addr.val;
        break;
    }

    // Do per-core work
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        processor_info_t* processor_info = &smp_info->processors[i];

        // Skip the BSP
        if (processor_info->apic_id == smp_get_current_core_apic_id(smp_info)) {
            continue;
        }
        // TODO(PT): Read a config/max_cpus.txt to decide when to stop booting APs
        break;

        printf("Booting core [idx %d], [APIC %d] [ID %d]\n", i, processor_info->apic_id, processor_info->processor_id);

        // Set up a virtual address space for the AP to use
        // Start off by cloning the BSP's address space, which has the high-memory remap
        // But ensure we create a new PML4E for CPU-local storage
        vas_state_t* ap_vas = vas_clone_ex(boot_info->vas_kernel, false);
        // Identity map the low 4G. We need to identity map more than just the AP bootstrap pages because the PML4 will reference arbitrary frames.
        // TODO(PT): This will cause problems if any of the paging structures are allocated above 4GB...
        vas_map_range_exact(ap_vas, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PML4), &ap_vas->pml4_phys, sizeof(ap_vas->pml4_phys));

        // Allocate a stack for the AP to use
        int ap_stack_size = PAGE_SIZE * 4;
        void* ap_stack = kcalloc(1, ap_stack_size);
        uintptr_t ap_stack_top = (uintptr_t)ap_stack + ap_stack_size;
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_STACK_TOP), &ap_stack_top, sizeof(ap_stack_top));

        // Long mode GDT
        uintptr_t ap_long_mode_gdt_size = 0;
        gdt_descriptor_t* ap_long_mode_gdt = 0;
        tss_t* ap_long_mode_tss = 0;
        gdt_create_for_long_mode(&ap_long_mode_gdt, &ap_long_mode_gdt_size, &ap_long_mode_tss);

        // Create a new GDT pointer that we'll place in the bootstrap params page
        gdt_pointer_t ap_long_mode_low_memory_gdt_ptr = {
            .table_base = AP_BOOTSTRAP_PARAM_LONG_MODE_LOW_MEMORY_GDT + 8,
            .table_size = ap_long_mode_gdt_size - 1
        };
        // Copy the pointer
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_LOW_MEMORY_GDT), &ap_long_mode_low_memory_gdt_ptr, sizeof(gdt_pointer_t));
        // Copy the table
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_LOW_MEMORY_GDT + 8), ap_long_mode_gdt, ap_long_mode_gdt_size);

        // Copy another GDT to a high-memory location
        // This will be the 'final' GDT that we use
        // We need a low-memory version because the low-memory GDT will be loaded before paging is enabled
        // And we need a high-memory version because the low-memory identity map will be removed later
        uintptr_t gdt_frame_phys = pmm_alloc();
        gdt_pointer_t* ap_long_mode_high_memory_gdt_ptr = (gdt_pointer_t*)PMA_TO_VMA(gdt_frame_phys);
        ap_long_mode_high_memory_gdt_ptr->table_base = (uintptr_t)ap_long_mode_gdt;
        ap_long_mode_high_memory_gdt_ptr->table_size = ap_long_mode_gdt_size - 1;
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_HIGH_MEMORY_GDT), &ap_long_mode_high_memory_gdt_ptr, sizeof(uintptr_t));

        // Map the per-core kernel data
        uintptr_t cpu_specific_data_frame = pmm_alloc();
        // TODO(PT): Use a more direct API that doesn't try to mark the range as allocated, as the range is already allocated via the BSP's VAS
        vas_map_range_exact(ap_vas, CPU_CORE_DATA_BASE, PAGE_SIZE, cpu_specific_data_frame, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        cpu_core_private_info_t* cpu_core_info = (cpu_core_private_info_t*)PMA_TO_VMA(cpu_specific_data_frame);
        memset(cpu_core_info, 0, sizeof(cpu_core_private_info_t));
        cpu_core_info->processor_id = processor_info->processor_id;
        cpu_core_info->apic_id = processor_info->apic_id;
        cpu_core_info->base_vas = ap_vas;
        cpu_core_info->loaded_vas_state = ap_vas;
        cpu_core_info->tss = ap_long_mode_tss;
        cpu_core_info->local_apic_phys_addr = smp_info->local_apic_phys_addr.val;

        smp_boot_core(smp_info, processor_info);
        //break;
    }
}

void smp_map_bsp_private_info(void) {
    uintptr_t cpu_specific_data_frame = pmm_alloc();
    vas_map_range_exact(boot_info_get()->vas_kernel, CPU_CORE_DATA_BASE, PAGE_SIZE, cpu_specific_data_frame, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
    _mapped_cpu_info_in_bsp = true;
    cpu_private_info()->base_vas = boot_info_get()->vas_kernel;
    cpu_private_info()->loaded_vas_state = boot_info_get()->vas_kernel;
    cpu_private_info()->tss = bsp_tss();
}

cpu_core_private_info_t* cpu_private_info(void) {
    // If we haven't yet had a chance to map the CPU info to the BSP,
    // we're still in early boot and can't provide meaningful info yet
    if (!_mapped_cpu_info_in_bsp) {
        static cpu_core_private_info_t _early_boot_placeholder = {0};
        return &_early_boot_placeholder;
    }
    return (cpu_core_private_info_t*)CPU_CORE_DATA_BASE;
}

uintptr_t cpu_id(void) {
    return cpu_private_info()->processor_id;
}

task_small_t* cpu_idle_task(void) {
    return cpu_private_info()->idle_task;
}
