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
use core::mem;
use ffi_bindings::println;

mod structs;
mod utils;

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

    parse_xstd_at_phys_addr(1, root_header.xsdt_phys_addr());

    // Ensure this allocation isn't optimized away by accessing it at the end
    hack_to_occupy_some_heap_memory.push(1);
}

fn dump_system_description(tab_level: usize, desc: &SystemDescriptionHeader) {
    let tabs = get_tabs(tab_level);
    println!("{tabs}ACPI System Description Header:");
    println!("{tabs}\tSignature: {}", desc.signature());
    println!("{tabs}\tOEM ID   : {}", desc.oem_id());
    println!("{tabs}\tOEM TabID: {}", desc.oem_table_id());
}

fn parse_xstd_at_phys_addr(tab_level: usize, phys_addr: usize) {
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
    for entry in entries {
        let table_phys_addr = *entry as usize;
        let table = parse_system_description_at_phys_addr(table_phys_addr);
        println!(
            "{tabs}\t[{table_phys_addr:#016x} XSTD Entry \"{}\"]",
            table.signature()
        );
        if table.signature() == "APIC" {
            parse_apic_table(tab_level + 2, table_phys_addr);
        }
    }
}

fn parse_apic_table(tab_level: usize, phys_addr: usize) {
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
                0 => parse_processor_local_apic(
                    tab_level + 1,
                    parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                ),
                1 => parse_io_apic(
                    tab_level + 1,
                    parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                ),
                2 => parse_io_apic_interrupt_source_override(
                    tab_level + 1,
                    parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                ),
                4 => parse_apic_non_maskable_interrupt(
                    tab_level + 1,
                    parse_struct_at_virt_addr(interrupt_controller_body_ptr),
                ),
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
}

fn parse_processor_local_apic(tab_level: usize, processor_local_apic: &ProcessorLocalApic) {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[ProcessorLocalApic P#{}, APIC#{}, F#{:#08x}]",
        processor_local_apic.processor_id(),
        processor_local_apic.apic_id(),
        processor_local_apic.flags()
    );
}

fn parse_io_apic(tab_level: usize, io_apic: &IoApic) {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[IoApic ID {}, PhysAddr {:#016x}, IntBase {}]",
        io_apic.id(),
        io_apic.apic_phys_addr(),
        io_apic.global_system_interrupt_base()
    );
}

fn parse_io_apic_interrupt_source_override(
    tab_level: usize,
    io_apic_interrupt_source_override: &IoApicInterruptSourceOverride,
) {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[IoApicInterruptSourceOverride BusSrc {}, IrqSrc {}, SysInt {}]",
        io_apic_interrupt_source_override.bus_source(),
        io_apic_interrupt_source_override.irq_source(),
        io_apic_interrupt_source_override.global_system_interrupt()
    );
}

fn parse_apic_non_maskable_interrupt(
    tab_level: usize,
    non_maskable_interrupt: &ApicNonMaskableInterrupt,
) {
    let tabs = get_tabs(tab_level);
    println!(
        "{tabs}[ApicNonMaskableInterrupt P#{}, Flags {:#04x}, Lint {}]",
        non_maskable_interrupt.for_processor_id(),
        non_maskable_interrupt.flags(),
        non_maskable_interrupt.lint()
    );
}

fn parse_system_description_at_phys_addr(phys_addr: usize) -> &'static SystemDescriptionHeader {
    parse_struct_at_phys_addr(phys_addr)
}
