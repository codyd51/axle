#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

extern crate alloc;
extern crate libc;

use alloc::{boxed::Box, collections::BTreeMap, format, rc::Weak, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use axle_rt::{
    core_commands::{amc_map_physical_range, AmcMapPhysicalRangeRequest},
    ContainsEventField, ExpectsEventField,
};
use axle_rt_derive::ContainsEventField;
use core::{cell::RefCell, cmp};

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service, printf, println, AmcMessage,
};

const PCI_SERVICE_NAME: &str = "com.axle.pci_driver";
// TODO(PT): This should be read from the PCI bus
const _SATA_CONTROLLER_BUS: u32 = 0;
const _SATA_CONTROLLER_DEVICE: u32 = 5;
const _SATA_CONTROLLER_FUNCTION: u32 = 0;

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct PciReadConfigWordRequest {
    event: u32,
    bus: u32,
    device: u32,
    function: u32,
    config_word_offset: u32,
}

impl ExpectsEventField for PciReadConfigWordRequest {
    const EXPECTED_EVENT: u32 = 1;
}

impl PciReadConfigWordRequest {
    pub fn new(config_word_offset: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            bus: _SATA_CONTROLLER_BUS,
            device: _SATA_CONTROLLER_DEVICE,
            function: _SATA_CONTROLLER_FUNCTION,
            config_word_offset,
        }
    }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct PciReadConfigWordResponse {
    event: u32,
    config_word_contents: u32,
}

impl ExpectsEventField for PciReadConfigWordResponse {
    const EXPECTED_EVENT: u32 = 1;
}

// No need to be locally constructed, this comes from the PCI daemon
impl PciReadConfigWordResponse {}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct PciWriteConfigWordRequest {
    event: u32,
    bus: u32,
    device: u32,
    function: u32,
    config_word_offset: u32,
    new_value: u32,
}

impl ExpectsEventField for PciWriteConfigWordRequest {
    const EXPECTED_EVENT: u32 = 2;
}

impl PciWriteConfigWordRequest {
    pub fn new(config_word_offset: u32, new_value: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            bus: _SATA_CONTROLLER_BUS,
            device: _SATA_CONTROLLER_DEVICE,
            function: _SATA_CONTROLLER_FUNCTION,
            config_word_offset,
            new_value,
        }
    }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct PciWriteConfigWordResponse {
    event: u32,
}

impl ExpectsEventField for PciWriteConfigWordResponse {
    const EXPECTED_EVENT: u32 = 2;
}

// No need to be locally constructed, this comes from the PCI daemon
impl PciWriteConfigWordResponse {}

fn pci_config_word_read(word_offset: u32) -> u32 {
    let request = PciReadConfigWordRequest::new(word_offset);
    amc_message_send(PCI_SERVICE_NAME, request);
    // The PCI daemon should send back the value of the config word
    let response: AmcMessage<PciReadConfigWordResponse> = amc_message_await(Some(PCI_SERVICE_NAME));
    response.body().config_word_contents
}

fn pci_config_word_write(word_offset: u32, new_value: u32) {
    let request = PciWriteConfigWordRequest::new(word_offset, new_value);
    amc_message_send(PCI_SERVICE_NAME, request);
    // The PCI daemon will unblock us once the value has been written
    let _: AmcMessage<PciWriteConfigWordResponse> = amc_message_await(Some(PCI_SERVICE_NAME));
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.sata_driver");

    println!("SATA driver running!");
    unsafe {
        libc::usleep(1000);
    }
    println!("SATA driver slept!");

    println!("Enabling bus mastering bit...");
    let command_register_off = 0x04;
    let mut config_word_value = pci_config_word_read(command_register_off);
    println!("Prior value of config word: {config_word_value:08x}");
    config_word_value |= (1 << 0x02);
    pci_config_word_write(command_register_off, config_word_value);
    println!("Wrote bus master enable bit!");

    // AHCI base address is stored in PCI BAR5
    let ahci_phys_base_address_raw = pci_config_word_read(0x24);
    // Bits 0-12 are for flags
    let ahci_phys_base_address = (ahci_phys_base_address_raw & !0xfff) as usize;
    println!("AHCI base address: 0x{ahci_phys_base_address:16x}");

    let interrupt_info = pci_config_word_read(0x3c);
    let interrupt_pin = (interrupt_info >> 8) & 0xff;
    println!("AHCI interrupt pin: {interrupt_pin}");
    // From the spec:
    // > If the HBA is a single function PCI device,
    // then INTR.IPIN should be set to 01h to indicate the INTA# pin.
    assert_eq!(interrupt_pin, 0x01);

    // AHCI base + 4: Global HBA Control
    // Bit 31 is AHCI enable bit - turn it on to tell the HBA that all
    // communication will be over AHCI instead of legacy mechanisms
    // Also, should we set the interrupt enable bit (bit 01)?

    // Check the ports implemented 0x0c

    // Read BAR5
    // Map in the physical AHCI range
    let ahci_base_address = {
        let virt_addr = amc_map_physical_range(ahci_phys_base_address, 0x1000);
        virt_addr as *mut u8
    };
    println!("Mapped AHCI range to virt {ahci_base_address:p}");

    loop {}

    0
}
