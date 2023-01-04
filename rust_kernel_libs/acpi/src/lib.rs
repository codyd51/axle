#![no_std]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate ffi_bindings;

use crate::structs::{
    ApicNonMaskableInterrupt, ExtendedSystemDescriptionHeader, InterruptControllerHeader, IoApic,
    IoApicInterruptSourceOverride, MultiApicDescriptionTable, ProcessorLocalApic,
    RootSystemDescriptionHeader, SystemDescriptionHeader,
};
use crate::utils::{get_tabs, parse_struct_at_phys_addr, parse_struct_at_virt_addr};
use alloc::string::String;
use alloc::vec;
use alloc::vec::Vec;
use core::fmt::{Debug, Formatter};
use core::mem;
use ffi_bindings::println;

mod structs;
mod utils;

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub struct PhysAddr(usize);

impl Debug for PhysAddr {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Phys[{:#016x}]", self.0)
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub struct VirtAddr(usize);

impl Debug for VirtAddr {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Virt[{:#016x}]", self.0)
    }
}

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
    // PT: Small trick to avoid spamming the syslogs
    // At the time of writing, the println! calls are cause the first kernel heap malloc/free, which
    // leads to lots of logs on each invocation as the first heap memory block is created/destroyed.
    let mut hack_to_occupy_some_heap_memory: Vec<u8> = Vec::with_capacity(16);

    println!("[{phys_addr:#016x} ACPI RootSystemDescription]");
    let root_header: &RootSystemDescriptionHeader = parse_struct_at_phys_addr(phys_addr);
    assert_eq!(root_header.signature(), "RSD PTR ");
    // Only ACPI 2.0 is supported, for now
    assert_eq!(root_header.revision(), 2);

    let info = parse_xstd_at_phys_addr(1, root_header.xsdt_phys_addr());
    println!("Got SMP info {info:?}");

    // Ensure this allocation isn't optimized away by accessing it at the end
    hack_to_occupy_some_heap_memory.push(1);
}

fn parse_xstd_at_phys_addr(tab_level: usize, phys_addr: usize) -> AcpiSmpInfo {
    let tabs = get_tabs(tab_level);
    let extended_system_desc: &ExtendedSystemDescriptionHeader =
        parse_struct_at_phys_addr(phys_addr);
    let extended_header = extended_system_desc.base;
    println!("{tabs}[{phys_addr:#016x} XSTD]");
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
        let table_phys_addr = *entry as usize;
        let table = parse_system_description_at_phys_addr(table_phys_addr);
        println!(
            "{tabs}\t[{table_phys_addr:#016x} XSTD Entry \"{}\"]",
            table.signature()
        );
        if table.signature() == "APIC" {
            ret = Some(parse_apic_table(tab_level + 2, table_phys_addr));
        }
    }

    ret.unwrap()
}

fn parse_apic_table(tab_level: usize, phys_addr: usize) -> AcpiSmpInfo {
    let tabs = get_tabs(tab_level);
    let apic_header: &MultiApicDescriptionTable = parse_struct_at_phys_addr(phys_addr);
    println!(
        "{tabs}[{:#016x} IntController Flags {:#08x}]",
        apic_header.local_interrupt_controller_phys_addr(),
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
            let interrupt_controller_header: &InterruptControllerHeader =
                parse_struct_at_virt_addr(interrupt_controller_header_ptr as usize);
            //println!("\t\t\tFound interrupt controller header {interrupt_controller_header:?}");
            let interrupt_controller_body_ptr = interrupt_controller_header_ptr
                .offset(mem::size_of::<InterruptControllerHeader>() as isize)
                as usize;

            match interrupt_controller_header.entry_type {
                0 => {
                    let processor = parse_processor_local_apic(
                        tab_level + 1,
                        parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                    );
                    processors.push(processor);
                }
                1 => {
                    io_apic_addr = Some(parse_io_apic(
                        tab_level + 1,
                        parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                    ));
                }
                2 => {
                    let interrupt_override = parse_io_apic_interrupt_source_override(
                        tab_level + 1,
                        parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                    );
                    interrupt_source_overrides.push(interrupt_override);
                }
                4 => {
                    let non_maskable_interrupt = parse_apic_non_maskable_interrupt(
                        tab_level + 1,
                        parse_struct_at_virt_addr(interrupt_controller_body_ptr),
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

    AcpiSmpInfo::new(
        PhysAddr(apic_header.local_interrupt_controller_phys_addr() as _),
        processors,
        io_apic_addr.unwrap(),
        interrupt_source_overrides,
        non_maskable_interrupts,
    )
}

fn parse_processor_local_apic(
    tab_level: usize,
    processor_local_apic: &ProcessorLocalApic,
) -> ProcessorInfo {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[ProcessorLocalApic P#{}, APIC#{}, F#{:#08x}]",
        processor_local_apic.processor_id(),
        processor_local_apic.apic_id(),
        processor_local_apic.flags()
    );
    ProcessorInfo::new(
        processor_local_apic.processor_id() as _,
        processor_local_apic.apic_id() as _,
    )
}

fn parse_io_apic(tab_level: usize, io_apic: &IoApic) -> PhysAddr {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[IoApic ID {}, PhysAddr {:#016x}, IntBase {}]",
        io_apic.id(),
        io_apic.apic_phys_addr(),
        io_apic.global_system_interrupt_base()
    );
    PhysAddr(io_apic.apic_phys_addr() as _)
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

fn parse_system_description_at_phys_addr(phys_addr: usize) -> &'static SystemDescriptionHeader {
    parse_struct_at_phys_addr(phys_addr)
}
