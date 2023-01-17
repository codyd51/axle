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

    // Set up the BSP's per-core data structure
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        processor_info_t *processor_info = &smp_info->processors[i];
        if (processor_info->apic_id != smp_get_current_core_apic_id(smp_info)) {
            continue;
        }

        // Found the BSP's processor info!
        cpu_core_private_info_t* cpu_core_info = cpu_private_info();
        memcpy(&cpu_core_info->processor_info, processor_info, sizeof(processor_info_t));
        break;
    }

    // Do per-core work
    for (uintptr_t i = 0; i < smp_info->processor_count; i++) {
        printf("Booting core idx %d\n", i);
        processor_info_t* processor_info = &smp_info->processors[i];
        printf("Processor info %d %d\n", processor_info->apic_id, processor_info->processor_id);

        // Skip the BSP
        if (processor_info->apic_id == smp_get_current_core_apic_id(smp_info)) {
            printf("Skipping BSP\n");
            continue;
        }

        // Set up a virtual address space for the AP to use
        // Start off by cloning the BSP's address space, which has the high-memory remap
        // But ensure we create a new PML4E for CPU-local storage
        printf("Cloning BSP VAS...\n");
        vas_state_t* ap_vas = vas_clone_ex(boot_info->vas_kernel, false);
        printf("Finished clone BSP VAS!\n");
        // Identity map the low 4G. We need to identity map more than just the AP bootstrap pages because the PML4 will reference arbitrary frames.
        // TODO(PT): This will cause problems if any of the paging structures are allocated above 4GB...
        vas_map_range_exact(ap_vas, 0x0, (1024LL * 1024LL * 1024LL * 4LL), 0x0, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        printf("Finished mapping 4GB\n");
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_PML4), &ap_vas->pml4_phys, sizeof(ap_vas->pml4_phys));

        // Allocate a stack for the AP to use
        printf("Allocating stack...\n");
        int ap_stack_size = PAGE_SIZE * 4;
        void* ap_stack = calloc(1, ap_stack_size);
        uintptr_t ap_stack_top = (uintptr_t)ap_stack + ap_stack_size;
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_STACK_TOP), &ap_stack_top, sizeof(ap_stack_top));

        printf("Finished create stack\n");

        // Long mode GDT
        uintptr_t ap_long_mode_gdt_size = 0;
        gdt_descriptor_t* ap_long_mode_gdt = 0;
        tss_t* ap_long_mode_tss = 0;
        gdt_create_for_long_mode(&ap_long_mode_gdt, &ap_long_mode_gdt_size, &ap_long_mode_tss);

        printf("Finished create long mode GDT\n");

        // Create a new GDT pointer that we'll place in the bootstrap params page
        gdt_pointer_t ap_long_mode_gdt_ptr = {
            .table_base = AP_BOOTSTRAP_PARAM_LONG_MODE_GDT + 8,
            .table_size = ap_long_mode_gdt_size
        };
        // Copy the pointer
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_GDT), &ap_long_mode_gdt_ptr, sizeof(gdt_pointer_t));
        // Copy the table
        memcpy((void*)PMA_TO_VMA(AP_BOOTSTRAP_PARAM_LONG_MODE_GDT + 8), ap_long_mode_gdt, ap_long_mode_gdt_size);

        printf("Finished mapping gdt etc\n");

        // Map the per-core kernel data
        uintptr_t cpu_specific_data_frame = pmm_alloc();
        printf("Got frame\n");
        // TODO(PT): Use a more direct API that doesn't try to mark the range as allocated, as the range is already allocated via the BSP's VAS
        vas_map_range_exact(ap_vas, CPU_CORE_DATA_BASE, PAGE_SIZE, cpu_specific_data_frame, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
        printf("Got mapping\n");
        cpu_core_private_info_t* cpu_core_info = (cpu_core_private_info_t*)PMA_TO_VMA(cpu_specific_data_frame);
        memset(cpu_core_info, 0, sizeof(cpu_core_private_info_t));
        memcpy(&cpu_core_info->processor_info, processor_info, sizeof(processor_info_t));
        cpu_core_info->base_vas = ap_vas;
        cpu_core_info->loaded_vas_state = ap_vas;

        printf("\tCalling smp_boot_core...\n");
        smp_boot_core(smp_info, processor_info);
        break;
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

void smp_map_bsp_private_info(void) {
    uintptr_t cpu_specific_data_frame = pmm_alloc();
    vas_map_range_exact(boot_info_get()->vas_kernel, CPU_CORE_DATA_BASE, PAGE_SIZE, cpu_specific_data_frame, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_KERNEL);
    _mapped_cpu_info_in_bsp = true;
    cpu_private_info()->base_vas = boot_info_get()->vas_kernel;
    cpu_private_info()->loaded_vas_state = boot_info_get()->vas_kernel;
}

cpu_core_private_info_t* cpu_private_info(void) {
    // If we haven't yet had a chance to map the CPU info to the BSP,
    // we're still in early boot and can't provide meaningful info yet
    if (!_mapped_cpu_info_in_bsp) {
        static cpu_core_private_info_t _early_boot_placeholder = {
            .processor_info = {
                .apic_id = 0,
                .processor_id = 0
            },
            .base_vas = NULL,
            .loaded_vas_state = NULL,
            .scheduler_enabled = false,
            .current_task = NULL,
        };
        return &_early_boot_placeholder;
    }
    return (cpu_core_private_info_t*)CPU_CORE_DATA_BASE;
}

uintptr_t cpu_id(void) {
    return cpu_private_info()->processor_info.processor_id;
}
