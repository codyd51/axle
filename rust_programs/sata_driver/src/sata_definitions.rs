use bitvec::prelude::*;

#[repr(C)]
#[derive(Debug)]
pub struct AhciGenericHostControlBlock {
    pub host_capabilities: u32,
    pub global_host_control: u32,
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

/*
type SixFlagsBits = BitSlice<Local, u16>;

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct SixFlags {
  inner: u16,
};

impl SixFlags {
  pub fn eins(&self) -> &SixFlagsBits {
    &self.inner.bits()[0 .. 3]
  }

  pub fn eins_mut(&mut self) -> &mut SixFlagsBits {
    &mut self.inner.bits()[0 .. 3]
  }

  pub fn zwei(&self) -> &SixFlagsBits {
    &self.inner.bits()[3 .. 5]
  }

  pub fn zwei_mut(&mut self) -> &mut SixFlagsBits {
    &mut self.inner.bits()[3 .. 5]
  }

  //  you get the idea…
}
*/

type AhciCommandHeaderWord0Bits = BitSlice<Lsb0, u32>;

#[repr(C)]
pub struct AhciCommandHeaderWord0 {
    inner: u32,
}

#[repr(C)]
#[derive(Debug)]
pub struct AhciCommandHeader {
    // TODO(PT): Use the bitfield crate to encode the packed attributes in this field?
    pub word0: u32,
    //pub word0: AhciCommandHeaderWord0Bits,
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
