use axle_rt::{
    amc_message_await, amc_message_send, AmcMessage, ContainsEventField, ExpectsEventField,
};
use axle_rt_derive::ContainsEventField;

const PCI_SERVICE_NAME: &str = "com.axle.pci_driver";
// TODO(PT): This should be read from the PCI bus
const _SATA_CONTROLLER_BUS: u32 = 0;
const _SATA_CONTROLLER_DEVICE: u32 = 5;
const _SATA_CONTROLLER_FUNCTION: u32 = 0;
pub const AHCI_INTERRUPT_VECTOR: u32 = 42;

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

pub fn pci_config_word_read(word_offset: u32) -> u32 {
    let request = PciReadConfigWordRequest::new(word_offset);
    amc_message_send(PCI_SERVICE_NAME, request);
    // The PCI daemon should send back the value of the config word
    let response: AmcMessage<PciReadConfigWordResponse> = amc_message_await(Some(PCI_SERVICE_NAME));
    response.body().config_word_contents
}

pub fn pci_config_word_write(word_offset: u32, new_value: u32) {
    let request = PciWriteConfigWordRequest::new(word_offset, new_value);
    amc_message_send(PCI_SERVICE_NAME, request);
    // The PCI daemon will unblock us once the value has been written
    let _: AmcMessage<PciWriteConfigWordResponse> = amc_message_await(Some(PCI_SERVICE_NAME));
}
