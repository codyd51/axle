#include <stddef.h>
#include <stdint.h>

#include <kernel/boot_info.h>
#include <kernel/pmm/pmm.h>
#include <kernel/segmentation/gdt.h>
#include <kernel/segmentation/gdt_structures.h>
#include <kernel/multitasking/tasks/task_small.h>

#include <kernel/ap_bootstrap.h>

#include "smp.h"

uintptr_t smp_get_current_core_apic_id(smp_info_t* smp_info);
void smp_boot_core(smp_info_t* smp_info, processor_info_t* core);
void apic_init(smp_info_t* smp_info);
smp_info_t* acpi_parse_root_system_description(uintptr_t acpi_rsdp);

void ap_c_entry(void) {
    uint64_t stack_helper;
    printf("AP running C code %p!!!\n", &stack_helper);
    while (1) {}
}

void smp_init(void) {
    boot_info_t* boot_info = boot_info_get();

    // Copy the AP bootstrap from wherever it was loaded into physical memory into its bespoke location
    // This location matches where the compiled code expects to be loaded.
    // AP startup code must also be placed below 1MB, as APs start up in real mode.
    //
    // First, verify our assumption that the AP bootstrap fits into a page
    assert(boot_info->ap_bootstrap_size < PAGE_SIZE, "AP bootstrap was larger than a page!");

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
    // We need to create a new pointer as the existing one points to high memory
    gdt_pointer_t long_mode_gdt_ptr = {0};
    uint32_t relocated_long_mode_gdt_addr = AP_BOOTSTRAP_PARAM_LONG_MODE_GDT + 8;
    long_mode_gdt_ptr.table_base = (uintptr_t)relocated_long_mode_gdt_addr;
    long_mode_gdt_ptr.table_size = current_long_mode_gdt->table_size;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_GDT), &long_mode_gdt_ptr, sizeof(gdt_pointer_t));
    memcpy((void*)PMA_TO_VMA(relocated_long_mode_gdt_addr), (void*)current_long_mode_gdt->table_base, current_long_mode_gdt->table_size);

    // Copy the IDT pointer
    idt_pointer_t* current_idt = kernel_idt_pointer();
    // It's fine to copy the high-memory IDT as the bootstrap will enable paging before loading it
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_IDT), current_idt, sizeof(idt_pointer_t) + current_idt->table_size);

    // Copy the C entry point
    uintptr_t ap_c_entry_point_addr = (uintptr_t)&ap_c_entry;
    memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_C_ENTRY), &ap_c_entry_point_addr, sizeof(ap_c_entry_point_addr));

    printf("Bootloader provided RSDP 0x%x\n", boot_info->acpi_rsdp);

    // Parse the ACPI tables and store the SMP info in the global system state
    smp_info_t* smp_info = acpi_parse_root_system_description(boot_info->acpi_rsdp);
    boot_info->smp_info = smp_info;

    // Set up the BSP's APIC and the IO APIC
    apic_init(boot_info->smp_info);

    asm("sti");

    // Do per-core work
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        processor_info_t* processor_info = &smp_info->processors[i];

        // Skip the BSP
        if (processor_info->apic_id == smp_get_current_core_apic_id(smp_info)) {
            printf("Skipping BSP\n");
            continue;
        }

        // Set up a virtual address space
        pml4e_t* bsp_pml4 = (pml4e_t*)PMA_TO_VMA(boot_info->vas_kernel->pml4_phys);
        uint64_t ap_pml4_phys_addr = pmm_alloc();
        pml4e_t* ap_pml4 = (pml4e_t*)PMA_TO_VMA(ap_pml4_phys_addr);
        // Copy all memory mappings from the BSP virtual address space
        for (int j = 0; j < 512; j++) {
            ap_pml4[j] = bsp_pml4[j];
        }
        // Identity map the low 4G. We need to identity map more than just the AP bootstrap pages because the PML4 will reference arbitrary frames.
        // TODO(PT): This will cause problems if any of the paging structures are allocated above 4GB...
        void _map_region_4k_pages(pml4e_t* page_mapping_level4_virt, uint64_t vmem_start, uint64_t vmem_size, uint64_t phys_start, vas_range_access_type_t access_type, vas_range_privilege_level_t privilege_level);
        _map_region_4k_pages(ap_pml4, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        // Copy the PML4 pointer
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PML4), &ap_pml4_phys_addr, sizeof(ap_pml4_phys_addr));

        // Map a stack for the AP to use
        int ap_stack_page_count = 4;
        uintptr_t ap_stack_top = VAS_KERNEL_STACK_BASE + (ap_stack_page_count * PAGE_SIZE);
        for (int j = 0; j < ap_stack_page_count; j++) {
            uintptr_t frame_addr = pmm_alloc();
            printf("Allocated AP stack frame %p\n", frame_addr);
            uintptr_t page_addr = VAS_KERNEL_STACK_BASE + (j * PAGE_SIZE);
            _map_region_4k_pages(ap_pml4, page_addr, PAGE_SIZE, frame_addr, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        }
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_STACK_TOP), &ap_stack_top, sizeof(ap_stack_top));

        // Map the per-core kernel data
        uintptr_t cpu_specific_data_frame = pmm_alloc();
        _map_region_4k_pages(ap_pml4, CPU_CORE_DATA_BASE, PAGE_SIZE, cpu_specific_data_frame, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        memcpy((void*)PMA_TO_VMA(cpu_specific_data_frame), processor_info, sizeof(processor_info_t));

        smp_boot_core(smp_info, processor_info);
    }

    /*
    printf("Got SMP info ptr %p\n", smp_info);
    printf("\tLocal APIC addr %p\n", smp_info->local_apic_phys_addr.val);
    printf("\tIO APIC addr %p\n", smp_info->io_apic_phys_addr.val);
    printf("\tProcessor count %d\n", smp_info->processor_count);
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        printf("\t\tProcessor #%d: ProcID %d, APIC ID %d\n", i, smp_info->processors[i].processor_id, smp_info->processors[i].apic_id);
    }
    printf("\tInterrupt override count %d\n", smp_info->interrupt_override_count);
    for (uintptr_t i = 0; i < smp_info->interrupt_override_count; i++) {
        interrupt_override_info_t* info = &smp_info->interrupt_overrides[i];
        printf("\t\tIntOverride %d: Bus %d Irq %d Sys %d\n", i, info->bus_source, info->irq_source, info->sys_interrupt);
    }
    */

    // Boot the other APs
    //smp_bringup(boot_info->smp_info);
}
