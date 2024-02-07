#![no_std]
#![feature(format_args_nl)]
#![feature(cstr_from_bytes_until_nul)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate ffi_bindings;

use crate::apic::{
    apic_disable_pic, apic_signal_end_of_interrupt, cpu_core_private_info, local_apic_timer_start,
    InterProcessorInterruptDeliveryMode, InterProcessorInterruptDescription,
    InterProcessorInterruptDestination, IoApic, ProcessorLocalApic, RemapIrqDescription,
};
use crate::interrupts::{idt_allocate_vector, idt_set_free_vectors};
use crate::smp::smp_info_ref;
use crate::structs::{
    ApicNonMaskableInterrupt, ExtendedSystemDescriptionHeader, InterruptControllerHeader,
    IoApicInterruptSourceOverride, IoApicRaw, MultiApicDescriptionTable, ProcessorLocalApicRaw,
    RootSystemDescriptionHeader, SystemDescriptionHeader,
};
use crate::utils::{
    get_tabs, parse_struct_at_phys_addr, parse_struct_at_virt_addr, spin_for_delay_ms, PhysAddr,
    VirtRamRemapAddr,
};
use alloc::alloc::alloc;
use alloc::vec;
use alloc::vec::Vec;
use core::alloc::Layout;
use core::arch::asm;
use core::intrinsics::copy_nonoverlapping;
use core::mem;
use core::mem::{align_of, size_of};
use ffi_bindings::{
    amc_wake_sleeping_services, interrupt_setup_callback, println, task_switch, RegisterStateX86_64,
};

mod amc;
mod apic;
mod interrupts;
mod scheduler;
mod smp;
mod spinlocks;
mod structs;
mod utils;

#[repr(C)]
#[derive(Debug)]
pub struct ProcessorInfo {
    processor_id: usize,
    apic_id: usize,
}

impl ProcessorInfo {
    fn new(processor_id: usize, apic_id: usize) -> Self {
        Self {
            processor_id,
            apic_id,
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct InterruptOverride {
    bus_source: usize,
    irq_source: usize,
    sys_interrupt: usize,
}

impl InterruptOverride {
    fn new(bus_source: usize, irq_source: usize, sys_interrupt: usize) -> Self {
        Self {
            bus_source,
            irq_source,
            sys_interrupt,
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct NonMaskableInterrupt {
    for_processor_id: usize,
}

impl NonMaskableInterrupt {
    fn new(for_processor_id: usize) -> Self {
        Self { for_processor_id }
    }
}

#[derive(Debug)]
pub struct AcpiSmpInfo {
    local_apic_addr: PhysAddr,
    processors: Vec<ProcessorInfo>,
    io_apic_addr: PhysAddr,
    interrupt_overrides: Vec<InterruptOverride>,
    non_maskable_interrupts: Vec<NonMaskableInterrupt>,
}

impl AcpiSmpInfo {
    fn new(
        local_apic_addr: PhysAddr,
        processors: Vec<ProcessorInfo>,
        io_apic_addr: PhysAddr,
        interrupt_overrides: Vec<InterruptOverride>,
        non_maskable_interrupts: Vec<NonMaskableInterrupt>,
    ) -> Self {
        Self {
            local_apic_addr,
            processors,
            io_apic_addr,
            interrupt_overrides,
            non_maskable_interrupts,
        }
    }
}

/// A representation of the SMP state of the system that can be stored in axle's global system state and used from C
#[repr(C)]
#[derive(Debug)]
pub struct SmpInfo {
    local_apic_phys_addr: PhysAddr,
    io_apic_phys_addr: PhysAddr,
    local_apic_timer_int_vector: usize,
    /// VLAs follow
    processor_count: usize,
    processors: [ProcessorInfo; Self::MAX_PROCESSORS],
    interrupt_override_count: usize,
    interrupt_overrides: [InterruptOverride; Self::MAX_INTERRUPT_OVERRIDES],
}

impl SmpInfo {
    pub const MAX_PROCESSORS: usize = 64;
    pub const MAX_INTERRUPT_OVERRIDES: usize = 64;
}

#[no_mangle]
pub fn acpi_parse_root_system_description(phys_addr: usize) -> *mut SmpInfo {
    println!("[{phys_addr:#016x} ACPI RootSystemDescription]");
    let phys_addr = PhysAddr(phys_addr);
    let root_header: &RootSystemDescriptionHeader = parse_struct_at_phys_addr(phys_addr);
    assert_eq!(root_header.signature(), "RSD PTR ");
    // Only ACPI 2.0 is supported, for now
    assert_eq!(root_header.revision(), 2);

    let smp_info = parse_xstd_at_phys_addr(1, root_header.xsdt_phys_addr());
    // Populate the kernel-safe data structure describing the SMP state
    let processor_count = smp_info.processors.len();
    let interrupt_override_count = smp_info.interrupt_overrides.len();
    // No need to reserve extra space for the arrays as we preallocate a maximum size
    let layout = Layout::from_size_align(size_of::<SmpInfo>(), align_of::<usize>()).unwrap();
    unsafe {
        let mut s = alloc(layout) as *mut SmpInfo;
        (*s).local_apic_phys_addr = smp_info.local_apic_addr;
        (*s).io_apic_phys_addr = smp_info.io_apic_addr;

        (*s).processor_count = processor_count;
        copy_nonoverlapping(
            smp_info.processors.as_ptr(),
            (*s).processors.as_mut_ptr(),
            processor_count,
        );

        (*s).interrupt_override_count = interrupt_override_count;
        copy_nonoverlapping(
            smp_info.interrupt_overrides.as_ptr(),
            (*s).interrupt_overrides.as_mut_ptr(),
            interrupt_override_count,
        );

        s
    }
}

#[no_mangle]
pub unsafe fn apic_init(smp_info: *mut SmpInfo) {
    // 1. Disable the legacy PIC as we're going to use the APIC
    apic_disable_pic();

    // 2. Enable the boot processor's local APIC
    // Intel Software Developer's Manual 10.4.1: The Local APIC Block Diagram
    // APIC registers are memory-mapped to a 4-KByte region of the processor’s physical address
    // space with an initial starting address of FEE00000H.
    // For correct APIC operation, this address space must be mapped to an area of memory that
    // has been designated as strong uncacheable (UC)
    let boot_processor_local_apic = ProcessorLocalApic::new((*smp_info).local_apic_phys_addr);
    println!(
        "APIC local ID {} version {}",
        boot_processor_local_apic.id(),
        boot_processor_local_apic.version(),
    );
    boot_processor_local_apic.enable();

    let io_apic = IoApic::new((*smp_info).io_apic_phys_addr);
    println!(
        "IO APIC ID {} version {}, max redirections {}",
        io_apic.id(),
        io_apic.version(),
        io_apic.max_redirection_entry()
    );

    // Start off by mapping the IOAPIC pins to the vectors axle expects
    // axle's interrupt map:
    // 00-32: Reserved for CPU exceptions
    // 32-64: Reserved for legacy PIT (always masked)
    // 64+: Reserved for IOAPIC
    // See kernel/kernel/interrupts/
    // First, remap all the ISA interrupts
    // But keep track of which interrupt vectors have an override
    unsafe { asm!("cli") };
    let mut isa_irqs_to_remap: Vec<usize> = (0..16).collect();
    let ioapic_idt_base = 64_usize;
    let mut free_ioapic_redirection_entries: Vec<usize> = (ioapic_idt_base
        ..(ioapic_idt_base + (io_apic.max_redirection_entry() as usize + 1)))
        .collect();
    let mut free_idt_vectors: Vec<usize> = (ioapic_idt_base..ioapic_idt_base + 32).collect();

    // Process the overrides
    for int_source_override_idx in 0..(*smp_info).interrupt_override_count {
        let int_source_override = &(*(smp_info)).interrupt_overrides[int_source_override_idx];
        println!("Process int override {int_source_override:?}");
        let isa_irq_line = int_source_override.sys_interrupt;
        let idt_vec = ioapic_idt_base + int_source_override.irq_source;
        io_apic.remap_irq(RemapIrqDescription::new(
            isa_irq_line as u8,
            idt_vec as u8,
            boot_processor_local_apic.id(),
        ));
        // And note that we've mapped this ISA IRQ
        isa_irqs_to_remap.retain(|&irq_vec| irq_vec != isa_irq_line);
        // And don't try to identity map the IRQ it's overriding
        isa_irqs_to_remap.retain(|&irq_vec| irq_vec != int_source_override.irq_source);
        // And note that we've occupied an IOAPIC entry + IDT vector
        free_ioapic_redirection_entries.retain(|&int_vec| int_vec != idt_vec as _);
        free_idt_vectors.retain(|&int_vec| int_vec != idt_vec as _);
    }

    // Identity map the remaining ISA IRQs
    for isa_irq_line in isa_irqs_to_remap.drain(..) {
        println!("Identity map ISA IRQ line {isa_irq_line}");
        let idt_vec = ioapic_idt_base + isa_irq_line;
        io_apic.remap_irq(RemapIrqDescription::new(
            isa_irq_line as u8,
            idt_vec as u8,
            boot_processor_local_apic.id(),
        ));
        // And note that we've occupied an IOAPIC entry + IDT vector
        free_ioapic_redirection_entries.retain(|&int_vec| int_vec != idt_vec as _);
        free_idt_vectors.retain(|&int_vec| int_vec != idt_vec as _);
    }

    for remaining_redirection_entry in free_ioapic_redirection_entries.iter() {
        // TODO(PT): Store the free IOAPIC redirection entries somewhere
        println!("Remaining IOAPIC redirection entry {remaining_redirection_entry}");
    }
    for remaining_idt_vec in free_idt_vectors.iter() {
        println!("Free IDT vector {remaining_idt_vec}");
    }
    idt_set_free_vectors(&free_idt_vectors);
    let local_apic_timer_int_vector = idt_allocate_vector();
    (*smp_info).local_apic_timer_int_vector = local_apic_timer_int_vector;

    unsafe {
        interrupt_setup_callback(
            local_apic_timer_int_vector as u8,
            cpu_core_handle_local_apic_timer_fired,
        );
    }

    // Finally, enable interrupts
    unsafe { asm!("sti") };
}

extern "C" fn cpu_core_handle_local_apic_timer_fired(register_state: *const RegisterStateX86_64) {
    unsafe {
        let register_state_ref = &*register_state;
        apic_signal_end_of_interrupt(register_state_ref.int_no as u8);
        // Always kick off a task switch when the LAPIC timer fires
        task_switch();
    }
}

#[no_mangle]
pub unsafe fn smp_get_current_core_apic_id(smp_info: *const SmpInfo) -> usize {
    let current_core_apic = ProcessorLocalApic::new((*smp_info).local_apic_phys_addr);
    current_core_apic.id() as _
}

#[no_mangle]
pub unsafe fn smp_boot_core(smp_info: *const SmpInfo, core: *const ProcessorInfo) {
    // Since this runs before SMP bringup, we're definitely running on the BSP
    let boot_processor_local_apic = ProcessorLocalApic::new((*smp_info).local_apic_phys_addr);
    let apic_id = (*core).apic_id;

    // Boot the AP
    let ipi_dest = InterProcessorInterruptDestination::OtherProcessor(apic_id);
    println!("Sending INIT IPI to APIC #{apic_id}...");
    boot_processor_local_apic.send_ipi(InterProcessorInterruptDescription::new(
        0,
        InterProcessorInterruptDeliveryMode::Init,
        ipi_dest,
    ));
    spin_for_delay_ms(10);
    println!("Sending SIPI to APIC #{apic_id}...");
    // Tell the AP to start executing at 0x8000
    // See ap_bootstrap.md
    boot_processor_local_apic.send_ipi(InterProcessorInterruptDescription::new(
        8,
        InterProcessorInterruptDeliveryMode::Startup,
        ipi_dest,
    ));
    spin_for_delay_ms(2);
    println!("Sending second SIPI to APIC #{apic_id}...");
    boot_processor_local_apic.send_ipi(InterProcessorInterruptDescription::new(
        8,
        InterProcessorInterruptDeliveryMode::Startup,
        ipi_dest,
    ));
}

fn parse_xstd_at_phys_addr(tab_level: usize, phys_addr: PhysAddr) -> AcpiSmpInfo {
    let tabs = get_tabs(tab_level);
    let extended_system_desc: &ExtendedSystemDescriptionHeader =
        parse_struct_at_phys_addr(phys_addr);
    let extended_header = extended_system_desc.base;
    println!("{tabs}[{:#016x} XSTD]", phys_addr.0);
    assert_eq!(extended_header.signature(), "XSDT");

    let length = extended_header.length();
    let num_entries =
        (length as usize - size_of::<ExtendedSystemDescriptionHeader>()) / size_of::<u64>();

    let entries = unsafe {
        let entries_raw_slice = core::ptr::slice_from_raw_parts(
            core::ptr::addr_of!(extended_system_desc.entries),
            num_entries,
        );
        let entries: &[u64] = &*(entries_raw_slice as *const [u64]);
        entries
    };

    let mut ret = None;
    for entry in entries {
        let table_phys_addr = PhysAddr(*entry as usize);
        let table = parse_system_description_at_phys_addr(table_phys_addr);
        println!(
            "{tabs}\t[{:#016x} XSTD Entry \"{}\"]",
            table_phys_addr.0,
            table.signature(),
        );
        if table.signature() == "APIC" {
            ret = Some(parse_apic_table(tab_level + 2, table_phys_addr));
        }
    }

    ret.unwrap()
}

fn parse_apic_table(tab_level: usize, phys_addr: PhysAddr) -> AcpiSmpInfo {
    let tabs = get_tabs(tab_level);
    let apic_header: &MultiApicDescriptionTable = parse_struct_at_phys_addr(phys_addr);
    println!(
        "{tabs}[{:#016x} IntController Flags {:#08x}]",
        apic_header.local_interrupt_controller_phys_addr().0,
        apic_header.flags(),
    );

    let mut interrupt_controller_header_ptr =
        core::ptr::addr_of!(apic_header.interrupt_controller_headers) as *const u8;
    let interrupt_controllers_len =
        (apic_header.base.length() as usize) - mem::size_of::<MultiApicDescriptionTable>();

    let mut processors = vec![];
    let mut io_apic_addr = None;
    let mut interrupt_source_overrides = vec![];
    let mut non_maskable_interrupts = vec![];
    unsafe {
        let end_ptr = interrupt_controller_header_ptr.offset(interrupt_controllers_len as isize);
        loop {
            let interrupt_controller_header: &InterruptControllerHeader = parse_struct_at_virt_addr(
                VirtRamRemapAddr(interrupt_controller_header_ptr as usize),
            );
            //println!("\t\t\tFound interrupt controller header {interrupt_controller_header:?}");
            let interrupt_controller_body_ptr = interrupt_controller_header_ptr
                .offset(mem::size_of::<InterruptControllerHeader>() as isize)
                as usize;

            match interrupt_controller_header.entry_type {
                0 => {
                    let processor = parse_processor_local_apic(
                        tab_level + 1,
                        parse_struct_at_virt_addr(VirtRamRemapAddr(interrupt_controller_body_ptr)),
                    );
                    processors.push(processor);
                }
                1 => {
                    io_apic_addr = Some(parse_io_apic(
                        tab_level + 1,
                        parse_struct_at_virt_addr(VirtRamRemapAddr(interrupt_controller_body_ptr)),
                    ));
                }
                2 => {
                    let interrupt_override = parse_io_apic_interrupt_source_override(
                        tab_level + 1,
                        parse_struct_at_virt_addr(VirtRamRemapAddr(interrupt_controller_body_ptr)),
                    );
                    interrupt_source_overrides.push(interrupt_override);
                }
                4 => {
                    let non_maskable_interrupt = parse_apic_non_maskable_interrupt(
                        tab_level + 1,
                        parse_struct_at_virt_addr(VirtRamRemapAddr(interrupt_controller_body_ptr)),
                    );
                    non_maskable_interrupts.push(non_maskable_interrupt);
                }
                _ => println!(
                    "{tabs}\t[Unknown APIC controller type {}",
                    interrupt_controller_header.entry_type
                ),
            }

            interrupt_controller_header_ptr = interrupt_controller_header_ptr
                .offset(interrupt_controller_header.entry_len as isize);
            if interrupt_controller_header_ptr >= end_ptr {
                break;
            }
        }
        println!("\t\tFinished parsing interrupt controllers");
    }

    // TODO(PT): Turn this into a lazy_static?
    AcpiSmpInfo::new(
        apic_header.local_interrupt_controller_phys_addr(),
        processors,
        io_apic_addr.unwrap(),
        interrupt_source_overrides,
        non_maskable_interrupts,
    )
}

fn parse_processor_local_apic(
    tab_level: usize,
    processor_local_apic: &ProcessorLocalApicRaw,
) -> ProcessorInfo {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[ProcessorLocalApicRaw P#{}, APIC#{}, F#{:#08x}]",
        processor_local_apic.processor_id(),
        processor_local_apic.apic_id(),
        processor_local_apic.flags()
    );
    ProcessorInfo::new(
        processor_local_apic.processor_id() as _,
        processor_local_apic.apic_id() as _,
    )
}

fn parse_io_apic(tab_level: usize, io_apic: &IoApicRaw) -> PhysAddr {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[IoApic ID {}, PhysAddr {:#016x}, IntBase {}]",
        io_apic.id(),
        io_apic.apic_phys_addr().0,
        io_apic.global_system_interrupt_base()
    );
    io_apic.apic_phys_addr()
}

fn parse_io_apic_interrupt_source_override(
    tab_level: usize,
    io_apic_interrupt_source_override: &IoApicInterruptSourceOverride,
) -> InterruptOverride {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[IoApicInterruptSourceOverride BusSrc {}, IrqSrc {}, SysInt {}]",
        io_apic_interrupt_source_override.bus_source(),
        io_apic_interrupt_source_override.irq_source(),
        io_apic_interrupt_source_override.global_system_interrupt()
    );
    InterruptOverride::new(
        io_apic_interrupt_source_override.bus_source() as _,
        io_apic_interrupt_source_override.irq_source() as _,
        io_apic_interrupt_source_override.global_system_interrupt() as _,
    )
}

fn parse_apic_non_maskable_interrupt(
    tab_level: usize,
    non_maskable_interrupt: &ApicNonMaskableInterrupt,
) -> NonMaskableInterrupt {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[ApicNonMaskableInterrupt P#{}, Flags {:#04x}, Lint {}]",
        non_maskable_interrupt.for_processor_id(),
        non_maskable_interrupt.flags(),
        non_maskable_interrupt.lint()
    );
    NonMaskableInterrupt::new(non_maskable_interrupt.for_processor_id() as _)
}

fn parse_system_description_at_phys_addr(phys_addr: PhysAddr) -> &'static SystemDescriptionHeader {
    parse_struct_at_phys_addr(phys_addr)
}
