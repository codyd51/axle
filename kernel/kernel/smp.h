#ifndef SMP_H
#define SMP_H

#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/segmentation/gdt.h>

// PT: This file contains some FFI bindings for Rust code, which must match the layouts defined in the Rust module

#define MAX_PROCESSORS 64
#define MAX_INTERRUPT_OVERRIDES 64

// Stores a cpu_core_private_info_t
#define CPU_CORE_DATA_BASE 0xFFFFA00000000000LL

typedef struct phys_addr {
    uintptr_t val;
} phys_addr_t;

typedef struct processor_info {
    uintptr_t processor_id;
    uintptr_t apic_id;
} processor_info_t;

typedef struct cpu_core_private_info {
    uintptr_t processor_id;
    uintptr_t apic_id;
    uintptr_t local_apic_phys_addr;
    vas_state_t* base_vas;
    vas_state_t* loaded_vas_state;
    task_small_t* current_task;
    bool scheduler_enabled;
    tss_t* tss;
    uintptr_t lapic_timer_ticks_per_ms;
    task_small_t* idle_task;
} cpu_core_private_info_t;

typedef struct interrupt_override_info {
    uintptr_t bus_source;
    uintptr_t irq_source;
    uintptr_t sys_interrupt;
} interrupt_override_info_t;

typedef struct smp_info {
    phys_addr_t local_apic_phys_addr;
    phys_addr_t io_apic_phys_addr;
    // All cores use the same interrupt vector for the local APIC timer callback
    uint64_t local_apic_timer_int_vector;
    // VLAs follow
    uintptr_t processor_count;
    processor_info_t processors[MAX_PROCESSORS];
    uintptr_t interrupt_override_count;
    interrupt_override_info_t interrupt_overrides[MAX_INTERRUPT_OVERRIDES];
} smp_info_t;

void smp_init(void);
void smp_map_bsp_private_info(void);
void local_apic_configure_timer(void);

cpu_core_private_info_t* cpu_private_info(void);
uintptr_t cpu_id(void);
task_small_t* cpu_idle_task(void);

#endif
