#ifndef SMP_H
#define SMP_H

// PT: Mostly FFI bindings for Rust code
// PT: Must match the layouts defined in the Rust modulee

#define MAX_PROCESSORS 64
#define MAX_INTERRUPT_OVERRIDES 64

#define CPU_CORE_DATA_BASE 0xFFFFB00000000000LL

typedef struct phys_addr {
    uintptr_t val;
} phys_addr_t;

typedef struct processor_info {
    uintptr_t processor_id;
    uintptr_t apic_id;
} processor_info_t;

typedef struct interrupt_override_info {
    uintptr_t bus_source;
    uintptr_t irq_source;
    uintptr_t sys_interrupt;
} interrupt_override_info_t;

typedef struct smp_info {
    phys_addr_t local_apic_phys_addr;
    phys_addr_t io_apic_phys_addr;
    // VLAs follow
    uintptr_t processor_count;
    processor_info_t processors[MAX_PROCESSORS];
    uintptr_t interrupt_override_count;
    interrupt_override_info_t interrupt_overrides[MAX_INTERRUPT_OVERRIDES];
} smp_info_t;

void smp_init(void);

#endif
