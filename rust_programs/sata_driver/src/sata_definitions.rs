use core::ptr::write_volatile;

use alloc::vec;
use alloc::{format, string::String, vec::Vec};
use axle_rt::{
    core_commands::{amc_alloc_physical_range, PhysRangeMapping},
    println,
};
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

impl AhciGenericHostControlBlock {
    pub fn clear_interrupt_status_mask(&mut self) {
        // Clear the top level ports-with-interrupts-to-service mask
        unsafe {
            write_volatile(&raw mut (*self).interrupt_status, self.interrupt_status);
        }
    }
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

impl AhciPortBlock {
    pub fn clear_interrupt_status_mask(&mut self) {
        // Clear the port's interrupts-to-service mask
        unsafe {
            write_volatile(&raw mut (*self).interrupt_status, self.interrupt_status);
        }
    }
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
    command_table_desc_base: u32,
    command_table_desc_base_upper: u32,
    pub reserved1: u32,
    pub reserved2: u32,
    pub reserved3: u32,
    pub reserved4: u32,
}

impl AhciCommandHeader {
    pub fn set_command_table_desc_base(&mut self, addr: u64) {
        self.command_table_desc_base = (addr & 0xffffffff) as u32;
        self.command_table_desc_base_upper = (addr >> 32) as u32;
    }

    pub fn command_table_desc_base(&self) -> u64 {
        ((self.command_table_desc_base_upper << 32) as u64) | (self.command_table_desc_base as u64)
    }
}

#[repr(u8)]
#[derive(Debug, Copy, Clone)]
pub enum CommandOpcode {
    ReadDmaExt = 0x25,
    WriteDmaExt = 0x35,
    IdentifyDevice = 0xEC,
}

// See Serial ATA 1.0, §8.5.2, Figure 58 – Register - Host to Device FIS layout
#[derive(Debug)]
pub struct HostToDeviceFIS(BitArray<[u32; 5], Lsb0>);

impl HostToDeviceFIS {
    // TODO(PT): Revise this lifetime
    pub fn from_virt_addr(virt_addr: usize) -> &'static mut Self {
        let command_fis = {
            let ptr = virt_addr as *mut HostToDeviceFIS;
            unsafe { &mut *ptr }
        };
        command_fis.set_command_fis_type();
        command_fis.set_addressing_mode();
        command_fis
    }

    pub fn init(&mut self) {
        self.set_command_fis_type()
    }

    fn set_command_fis_type(&mut self) {
        self.0[0..8].store::<u8>(0x27)
    }

    fn set_addressing_mode(&mut self) {
        // Ref: https://wiki.osdev.org/ATA_PIO_Mode
        // Always use LBA addressing
        self.0[56..64].store::<u8>(1 << 6)
    }

    pub fn set_command(&mut self, command: CommandOpcode) {
        self.0[16..24].store::<u8>(command as u8);
    }

    pub fn set_is_command(&mut self, is_command: bool) {
        self.0.set(15, is_command)
    }

    pub fn set_sector_count(&mut self, sector_count: u16) {
        self.0[96..104].store::<u8>((sector_count & 0xff) as u8);
        self.0[104..112].store::<u8>(((sector_count >> 8) & 0xff) as u8);
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct RawPhysRegionDescriptor {
    data_base_address: u32,
    data_base_address_upper: u32,
    reserved: u32,
    pub word3: BitArray<u32, Lsb0>,
}

impl RawPhysRegionDescriptor {
    pub fn set_data_base_address(&mut self, addr: u64) {
        assert!(addr & 0b1 == 0, "Data base address must be byte-aligned");

        self.data_base_address = (addr & 0xffffffff) as u32;
        self.data_base_address_upper = (addr >> 32) as u32;
    }

    pub fn data_base_address(&self) -> u64 {
        ((self.data_base_address_upper << 32) as u64) | (self.data_base_address as u64)
    }

    pub fn interrupt_on_completion(&self) -> bool {
        *self.word3.get(31).unwrap()
    }

    pub fn set_interrupt_on_completion(&mut self, value: bool) {
        self.word3.set(31, value)
    }

    pub fn byte_count(&self) -> u32 {
        self.word3[0..21].load::<u32>()
    }

    pub fn set_byte_count(&mut self, value: u32) {
        // LSB of stored byte count must always be 1 to indicate an even byte count
        // Therefore, ensure the input is even and more than 1, then subtract one from it
        assert!(
            value % 2 == 0 && value >= 2,
            "Value must be at least 2 and even",
        );
        self.word3[0..21].store::<u32>(value - 1)
    }
}

// See ATA8-ACS, §7.16.7, Table 22 – IDENTIFY DEVICE data
#[derive(Debug)]
pub struct IdentifyDeviceData(BitArray<[u16; 256], Lsb0>);

impl IdentifyDeviceData {
    pub fn serial_number(&self) -> String {
        // Words 10-19, LSB
        let mut out = vec![];
        'outer: for word in self.0.data[10..19].iter() {
            let bytes = word.to_be_bytes();
            for byte in bytes {
                // Stop parsing once we hit a space character
                if byte == 0x20 {
                    break 'outer;
                }
                out.push(byte as char);
            }
        }
        out.into_iter().collect()
    }
}
