use alloc::string::String;

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#root-system-description-pointer-rsdp-structure
pub struct RootSystemDescriptionHeader {
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
    pub fn signature(&self) -> String {
        String::from_utf8_lossy(&self.signature).into_owned()
    }

    pub fn oem_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_id).into_owned()
    }

    pub fn table_length(&self) -> usize {
        self.table_length as usize
    }

    pub fn rsdt_phys_addr(&self) -> usize {
        self.rsdt_phys_addr as usize
    }

    pub fn xsdt_phys_addr(&self) -> usize {
        self.xsdt_phys_addr as usize
    }

    pub fn revision(&self) -> u8 {
        self.revision
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#system-description-table-header
pub struct SystemDescriptionHeader {
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
    pub fn signature(&self) -> String {
        String::from_utf8_lossy(&self.signature).into_owned()
    }

    pub fn oem_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_id).into_owned()
    }

    pub fn oem_table_id(&self) -> String {
        String::from_utf8_lossy(&self.oem_table_id).into_owned()
    }

    pub fn length(&self) -> u32 {
        self.length
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#extended-system-description-table-xsdt
pub struct ExtendedSystemDescriptionHeader {
    pub base: SystemDescriptionHeader,
    pub entries: [u64; 0],
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt
pub struct MultiApicDescriptionTable {
    pub base: SystemDescriptionHeader,
    local_interrupt_controller_phys_addr: u32,
    flags: u32,
    pub interrupt_controller_headers: [InterruptControllerHeader; 0],
}

impl MultiApicDescriptionTable {
    pub fn local_interrupt_controller_phys_addr(&self) -> u32 {
        self.local_interrupt_controller_phys_addr
    }

    pub fn flags(&self) -> u32 {
        self.flags
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
/// Ref: https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt
pub struct InterruptControllerHeader {
    pub entry_type: u8,
    pub entry_len: u8,
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
pub struct ProcessorLocalApic {
    processor_id: u8,
    apic_id: u8,
    flags: u32,
}

impl ProcessorLocalApic {
    pub fn processor_id(&self) -> u8 {
        self.processor_id
    }

    pub fn apic_id(&self) -> u8 {
        self.apic_id
    }

    pub fn flags(&self) -> u32 {
        self.flags
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
pub struct IoApic {
    id: u8,
    reserved: u8,
    apic_phys_addr: u32,
    global_system_interrupt_base: u32,
}

impl IoApic {
    pub fn id(&self) -> u8 {
        self.id
    }

    pub fn apic_phys_addr(&self) -> u32 {
        self.apic_phys_addr
    }

    pub fn global_system_interrupt_base(&self) -> u32 {
        self.global_system_interrupt_base
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
pub struct IoApicInterruptSourceOverride {
    bus_source: u8,
    irq_source: u8,
    global_system_interrupt: u32,
    flags: u16,
}

impl IoApicInterruptSourceOverride {
    pub fn bus_source(&self) -> u8 {
        self.bus_source
    }

    pub fn irq_source(&self) -> u8 {
        self.irq_source
    }

    pub fn global_system_interrupt(&self) -> u32 {
        self.global_system_interrupt
    }

    pub fn flags(&self) -> u16 {
        self.flags
    }
}

#[repr(packed)]
#[derive(Debug, Copy, Clone)]
pub struct ApicNonMaskableInterrupt {
    for_processor_id: u8,
    flags: u16,
    lint: u8,
}

impl ApicNonMaskableInterrupt {
    pub fn for_processor_id(&self) -> u8 {
        self.for_processor_id
    }

    pub fn flags(&self) -> u16 {
        self.flags
    }

    pub fn lint(&self) -> u8 {
        self.lint
    }
}
