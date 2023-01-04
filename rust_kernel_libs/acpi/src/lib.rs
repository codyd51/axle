#![no_std]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate ffi_bindings;

use alloc::string::String;
use alloc::vec;
use alloc::vec::Vec;
use core::mem;
use ffi_bindings::println;

/// PT: Matches the definitions in kernel/util/vmm
const KERNEL_MEMORY_BASE: usize = 0xFFFF800000000000;

/// Converts a physical address to its corresponding remapped address in high memory
fn phys_addr_to_remapped_high_memory_virt_addr(phys_addr: usize) -> usize {
    KERNEL_MEMORY_BASE + phys_addr
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#root-system-description-pointer-rsdp-structure
struct RootSystemDescriptionHeader {
    signature: [u8; 8],
    checksum: u8,
    oem_id: [u8; 6],
    revision: u8,
    rsdt_phys_addr: u32,
    table_length: u32,
    xsdt_phys_addr: u64,
    extended_checksum: u8,
    reserved: [u8; 3],
}

// Design note: We need getters for many fields rather than raw field access because many of the
// fields are unaligned, due to the packed structs. If we tried to directly use fields inside
// println!() this would implicitly create a reference, which is undefined for unaligned fields.
// Copying into a local works, though, so getters (that copy the field) get around this quirk.

impl RootSystemDescriptionHeader {
    fn signature(&self) -> String {
        String::from_utf8_lossy(&self.signature).into_owned()
    }

    fn oem_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_id).into_owned()
    }

    fn table_length(&self) -> usize {
        self.table_length as usize
    }

    fn rsdt_phys_addr(&self) -> usize {
        self.rsdt_phys_addr as usize
    }

    fn xsdt_phys_addr(&self) -> usize {
        self.xsdt_phys_addr as usize
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#system-description-table-header
struct SystemDescriptionHeader {
    signature: [u8; 4],
    length: u32,
    revision: u8,
    checksum: u8,
    oem_id: [u8; 6],
    oem_table_id: [u8; 8],
    oem_revision: [u8; 4],
    creator_id: [u8; 4],
    creator_revision: [u8; 4],
}

impl SystemDescriptionHeader {
    fn signature(&self) -> String {
        String::from_utf8_lossy(&self.signature).into_owned()
    }

    fn oem_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_id).into_owned()
    }

    fn oem_table_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_table_id).into_owned()
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#extended-system-description-table-xsdt
struct ExtendedSystemDescriptionHeader {
    base: SystemDescriptionHeader,
    entries: [u64; 0],
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt
struct MultiApicDescriptionTable {
    base: SystemDescriptionHeader,
    local_interrupt_controller_phys_addr: u32,
    flags: u32,
    interrupt_controller_headers: [InterruptControllerHeader; 0],
}

impl MultiApicDescriptionTable {
    fn local_interrupt_controller_phys_addr(&self) -> u32 {
        self.local_interrupt_controller_phys_addr
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt
struct InterruptControllerHeader {
    entry_type: u8,
    entry_len: u8,
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct ProcessorLocalApic {
    processor_id: u8,
    apic_id: u8,
    flags: u32,
}

impl ProcessorLocalApic {
    fn processor_id(&self) -> u8 {
        self.processor_id
    }

    fn apic_id(&self) -> u8 {
        self.apic_id
    }

    fn flags(&self) -> u32 {
        self.flags
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct IoApic {
    id: u8,
    reserved: u8,
    apic_phys_addr: u32,
    global_system_interrupt_base: u32,
}

impl IoApic {
    fn id(&self) -> u8 {
        self.id
    }

    fn apic_phys_addr(&self) -> u32 {
        self.apic_phys_addr
    }

    fn global_system_interrupt_base(&self) -> u32 {
        self.global_system_interrupt_base
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct IoApicInterruptSourceOverride {
    bus_source: u8,
    irq_source: u8,
    global_system_interrupt: u32,
    flags: u16,
}

impl IoApicInterruptSourceOverride {
    fn bus_source(&self) -> u8 {
        self.bus_source
    }

    fn irq_source(&self) -> u8 {
        self.irq_source
    }

    fn global_system_interrupt(&self) -> u32 {
        self.global_system_interrupt
    }

    fn flags(&self) -> u16 {
        self.flags
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct ApicNonMaskableInterrupt {
    for_processor_id: u8,
    flags: u16,
    lint: u8,
}

impl ApicNonMaskableInterrupt {
    fn for_processor_id(&self) -> u8 {
        self.for_processor_id
    }

    fn flags(&self) -> u16 {
        self.flags
    }

    fn lint(&self) -> u8 {
        self.lint
    }
}

pub fn parse_struct_at_virt_addr<T>(virt_addr: usize) -> &'static T {
    unsafe { &*(virt_addr as *const T) }
}

pub fn parse_struct_at_phys_addr<T>(phys_addr: usize) -> &'static T {
    let virt_addr = phys_addr_to_remapped_high_memory_virt_addr(phys_addr);
    parse_struct_at_virt_addr(virt_addr)
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
    assert_eq!(root_header.revision, 2);

    parse_xstd_at_phys_addr(1, root_header.xsdt_phys_addr());

    // Ensure this allocation isn't optimized away by accessing it at the end
    hack_to_occupy_some_heap_memory.push(1);
}

fn get_tabs(num: usize) -> String {
    "\t".repeat(num)
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

    let length = extended_header.length;
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
        "{tabs}[{:#016x} IntController]",
        apic_header.local_interrupt_controller_phys_addr()
    );

    let mut interrupt_controller_header_ptr =
        core::ptr::addr_of!(apic_header.interrupt_controller_headers) as *const u8;
    let interrupt_controllers_len =
        (apic_header.base.length as usize) - mem::size_of::<MultiApicDescriptionTable>();

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
