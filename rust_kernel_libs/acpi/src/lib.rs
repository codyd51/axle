#![no_std]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate ffi_bindings;

use crate::apic::{apic_disable_pic, apic_enable, IoApic, ProcessorLocalApic, RemapIrqDescription};
use crate::structs::{
    ApicNonMaskableInterrupt, ExtendedSystemDescriptionHeader, InterruptControllerHeader,
    IoApicInterruptSourceOverride, IoApicRaw, MultiApicDescriptionTable, ProcessorLocalApicRaw,
    RootSystemDescriptionHeader, SystemDescriptionHeader,
};
use crate::utils::{
    get_tabs, parse_struct_at_phys_addr, parse_struct_at_virt_addr, PhysAddr, VirtRamRemapAddr,
};
use alloc::vec;
use alloc::vec::Vec;
use core::mem;
use ffi_bindings::println;

mod apic;
mod structs;
mod utils;

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

#[no_mangle]
pub fn acpi_parse_root_system_description(phys_addr: usize) {
    println!("[{phys_addr:#016x} ACPI RootSystemDescription]");
    let phys_addr = PhysAddr(phys_addr);
    let root_header: &RootSystemDescriptionHeader = parse_struct_at_phys_addr(phys_addr);
    assert_eq!(root_header.signature(), "RSD PTR ");
    // Only ACPI 2.0 is supported, for now
    assert_eq!(root_header.revision(), 2);

    let smp_info = parse_xstd_at_phys_addr(1, root_header.xsdt_phys_addr());

    // 1. Disable the legacy PIC as we're going to use the APIC
    apic_disable_pic();

    // 2. Enable the boot processor's local APIC
    // Intel Software Developer's Manual 10.4.1: The Local APIC Block Diagram
    // APIC registers are memory-mapped to a 4-KByte region of the processor’s physical address
    // space with an initial starting address of FEE00000H.
    // For correct APIC operation, this address space must be mapped to an area of memory that
    // has been designated as strong uncacheable (UC)
    let boot_processor_local_apic = ProcessorLocalApic::new(smp_info.local_apic_addr);
    println!(
        "APIC local ID {} version {}",
        boot_processor_local_apic.id(),
        boot_processor_local_apic.version(),
    );
    boot_processor_local_apic.enable();

    let io_apic = IoApic::new(smp_info.io_apic_addr);
    println!(
        "IO APIC ID {} version {}, max redirections {}",
        io_apic.id(),
        io_apic.version(),
        io_apic.max_redirection_entry()
    );

    // Start off by mapping the first 16 IRQ vectors to the nominal axle IDT vector.
    // axle remaps IRQs to the interrupt number rebased by 32.
    // See kernel/kernel/interrupts/idt.c
    for i in 0..16 {
        io_apic.remap_irq(RemapIrqDescription::new(
            i,
            32 + i,
            boot_processor_local_apic.id(),
        ));
    }
    // Now that we've set up the base case, apply any requested interrupt source overrides
    for int_source_override in smp_info.interrupt_overrides.iter() {
        io_apic.remap_irq(RemapIrqDescription::new(
            int_source_override.sys_interrupt as u8,
            32 + int_source_override.irq_source as u8,
            boot_processor_local_apic.id(),
        ));
    }
    apic_enable();
}

fn parse_xstd_at_phys_addr(tab_level: usize, phys_addr: PhysAddr) -> AcpiSmpInfo {
    let tabs = get_tabs(tab_level);
    let extended_system_desc: &ExtendedSystemDescriptionHeader =
        parse_struct_at_phys_addr(phys_addr);
    let extended_header = extended_system_desc.base;
    println!("{tabs}[{:#016x} XSTD]", phys_addr.0);
    assert_eq!(extended_header.signature(), "XSDT");

    let length = extended_header.length();
    let num_entries = (length as usize - mem::size_of::<ExtendedSystemDescriptionHeader>())
        / mem::size_of::<u64>();

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
