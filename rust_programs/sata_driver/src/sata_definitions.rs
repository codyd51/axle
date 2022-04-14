use alloc::format;
use bitvec::prelude::*;

type AhciGlobalHostControlBits = BitArray<u32, Lsb0>;

#[repr(C)]
#[derive(Debug)]
pub struct AhciGenericHostControlBlock {
    pub host_capabilities: u32,
    pub global_host_control: AhciGlobalHostControlBits,
    pub interrupt_status: u32,
    pub ports_implemented: u32,
    pub version: u32,
    pub command_completion_coalescing_control: u32,
    pub command_completion_coalescing_ports: u32,
    pub enclosure_management_location: u32,
    pub enclosure_management_control: u32,
    pub host_capabilities_extended: u32,
    pub bios_os_handoff_control_and_status: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct AhciPortBlock {
    pub command_list_base: u32,
    pub command_list_base_upper: u32,
    pub frame_info_struct_base: u32,
    pub frame_info_struct_base_upper: u32,
    pub interrupt_status: u32,
    pub interrupt_enable: u32,
    pub command_and_status: u32,
    pub reserved: u32,
    pub task_file_data: u32,
    pub signature: u32,
    pub sata_status: u32,
    pub sata_control: u32,
    pub sata_error: u32,
    pub sata_active: u32,
    pub command_issue: u32,
    pub sata_notification: u32,
    pub switching_control: u32,
    pub deice_sleep: u32,
    // Reserved & vendor specific fields here
}

pub type AhciCommandHeaderWord0Bits2 = BitArray<u32, Lsb0>;

#[derive(Debug)]
pub struct AhciCommandHeaderWord0(BitArray<u32, Lsb0>);

impl AhciCommandHeaderWord0 {
    pub fn command_fis_len(&self) -> u32 {
        self.0[..5].load::<u32>()
    }

    pub fn set_command_fis_len(&mut self, value: u32) {
        self.0[..5].store::<u32>(value)
    }

    pub fn atapi(&self) -> bool {
        *self.0.get(5).unwrap()
    }

    pub fn set_atapi(&mut self, value: bool) {
        self.0.set(5, value)
    }

    pub fn write(&self) -> bool {
        *self.0.get(6).unwrap()
    }

    pub fn set_write(&mut self, value: bool) {
        self.0.set(6, value)
    }

    pub fn prefetchable(&self) -> bool {
        *self.0.get(7).unwrap()
    }

    pub fn set_prefetchable(&mut self, value: bool) {
        self.0.set(7, value)
    }

    pub fn reset(&self) -> bool {
        *self.0.get(8).unwrap()
    }

    pub fn set_reset(&mut self, value: bool) {
        self.0.set(8, value)
    }

    pub fn bist(&self) -> bool {
        *self.0.get(9).unwrap()
    }

    pub fn set_bist(&mut self, value: bool) {
        self.0.set(9, value)
    }

    pub fn clear_busy_upon_r_ok(&self) -> bool {
        *self.0.get(10).unwrap()
    }

    pub fn set_clear_busy_upon_r_ok(&mut self, value: bool) {
        self.0.set(10, value)
    }

    // Bit 11 is reserved, don't expose it

    pub fn port_multiplier_port(&self) -> u32 {
        self.0[12..16].load::<u32>()
    }

    pub fn set_port_multiplier_port(&mut self, value: u32) {
        self.0[12..16].store::<u32>(value)
    }

    pub fn phys_region_desc_table_len(&self) -> u32 {
        self.0[16..].load::<u32>()
    }

    pub fn set_phys_region_desc_table_len(&mut self, value: u32) {
        self.0[16..].store::<u32>(value)
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct AhciCommandHeader {
    pub word0: AhciCommandHeaderWord0,
    pub phys_region_desc_byte_count: u32,
    pub command_table_desc_base: u32,
    pub command_table_desc_base_upper: u32,
    pub reserved1: u32,
    pub reserved2: u32,
    pub reserved3: u32,
    pub reserved4: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct HostToDeviceFIS {
    // TODO(PT): Use the bitfield crate to encode the packed attributes in this field?
    pub word0: u32,
    pub word1: u32,
    pub word2: u32,
    pub word3: u32,
    pub word4: u32,
}
