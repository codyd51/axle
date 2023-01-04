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

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct IoApic {
    id: u8,
    reserved: u8,
    apic_phys_addr: u32,
    global_system_interrupt_base: u32,
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct IoApicInterruptSourceOverride {
    bus_source: u8,
    irq_source: u8,
    global_system_interrupt: u32,
    flags: u16,
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
struct ApicNonMaskableInterrupt {
    for_processor_id: u8,
    flags: u16,
    lint: u8,
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

    println!("Parsing ACPI RSDP at {phys_addr:#016x}");
    let root_header: &RootSystemDescriptionHeader = parse_struct_at_phys_addr(phys_addr);
    assert_eq!(root_header.signature(), "RSD PTR ");
    println!("\tOEM ID: {}", root_header.oem_id());
    // Only ACPI 2.0 is supported, for now
    assert_eq!(root_header.revision, 2);

    println!("\tRSDT @ {:#016x}", root_header.rsdt_phys_addr());
    println!("\t  Len  {:#016x}", root_header.table_length());
    println!("\tXSDT @ {:#016x}", root_header.xsdt_phys_addr());

    parse_xstd_at_phys_addr(root_header.xsdt_phys_addr());

    // Ensure this allocation isn't optimized away by accessing it at the end
    hack_to_occupy_some_heap_memory.push(1);
}

fn dump_system_description(tab_level: usize, desc: &SystemDescriptionHeader) {
    let tabs = "\t".repeat(tab_level);
    println!("{tabs}ACPI System Description Header:");
    println!("{tabs}\tSignature: {}", desc.signature());
    println!("{tabs}\tOEM ID   : {}", desc.oem_id());
    println!("{tabs}\tOEM TabID: {}", desc.oem_table_id());
}

fn parse_xstd_at_phys_addr(phys_addr: usize) {
    let extended_system_desc: &ExtendedSystemDescriptionHeader =
        parse_struct_at_phys_addr(phys_addr);
    let extended_header = extended_system_desc.base;
    println!("Got XSTD {extended_header:?}");
    assert_eq!(extended_header.signature(), "XSDT");
    println!("\tOEM ID: {}", extended_header.oem_id());
    println!("\tOEM ID: {}", extended_header.oem_table_id());

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
        println!("\tEntry {entry:x} {:p}", &entry);

        let table = parse_system_description_at_phys_addr(*entry as usize);
        dump_system_description(2, table);
        if table.signature() == "APIC" {
            println!("\t\tFound APIC table at {entry:x}!");
            let apic_header: &MultiApicDescriptionTable =
                parse_struct_at_phys_addr(*entry as usize);
            println!(
                "\t\tInterrupt controller address: {:x}",
                apic_header.local_interrupt_controller_phys_addr()
            );

            unsafe {
                let mut interrupt_controller_header_ptr =
                    core::ptr::addr_of!(apic_header.interrupt_controller_headers) as *const u8;
                let interrupt_controllers_len = (apic_header.base.length as usize)
                    - mem::size_of::<MultiApicDescriptionTable>();
                let end_ptr =
                    interrupt_controller_header_ptr.offset(interrupt_controllers_len as isize);
                println!("\t\tInterrupt controllers len: {interrupt_controllers_len}");
                loop {
                    let interrupt_controller_header: &InterruptControllerHeader =
                        parse_struct_at_virt_addr(interrupt_controller_header_ptr as usize);
                    println!(
                        "\t\t\tFound interrupt controller header {interrupt_controller_header:?}"
                    );
                    let interrupt_controller_body_ptr = interrupt_controller_header_ptr
                        .offset(mem::size_of::<InterruptControllerHeader>() as isize);

                    if interrupt_controller_header.entry_type == 0 {
                        let processor_apic: &ProcessorLocalApic =
                            parse_struct_at_virt_addr(interrupt_controller_body_ptr as usize);
                        println!("\t\t\t\tParsed ProcessorLocalApic {processor_apic:?}");
                    } else if interrupt_controller_header.entry_type == 1 {
                        let io_apic: &IoApic =
                            parse_struct_at_virt_addr(interrupt_controller_body_ptr as usize);
                        println!("\t\t\t\tParsed IoApic {io_apic:?}");
                    } else if interrupt_controller_header.entry_type == 2 {
                        let source_override: &IoApicInterruptSourceOverride =
                            parse_struct_at_virt_addr(interrupt_controller_body_ptr as usize);
                        println!(
                            "\t\t\t\tParsed IoApicInterruptSourceOverride {source_override:?}"
                        );
                    } else if interrupt_controller_header.entry_type == 4 {
                        let non_maskable_interrupt: &ApicNonMaskableInterrupt =
                            parse_struct_at_virt_addr(interrupt_controller_body_ptr as usize);
                        println!(
                            "\t\t\t\tParsed IoApicNonMaskableInterrupt {non_maskable_interrupt:?}"
                        );
                    } else {
                        println!("\t\t\t\tUnknown type");
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
    }
}

fn parse_system_description_at_phys_addr(phys_addr: usize) -> &'static SystemDescriptionHeader {
    parse_struct_at_phys_addr(phys_addr)
}
