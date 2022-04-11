#![no_std]
#![feature(start)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]
#![feature(default_alloc_error_handler)]

mod pci_messages;
mod sata_definitions;

extern crate alloc;
extern crate libc;

use alloc::{boxed::Box, collections::BTreeMap, format, rc::Weak, vec, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};

use bitvec::prelude::*;

use axle_rt::{
    core_commands::{
        amc_alloc_physical_range, amc_map_physical_range, AmcMapPhysicalRangeRequest, PhysVirtPair,
    },
    ContainsEventField, ExpectsEventField,
};
use axle_rt_derive::ContainsEventField;
use core::{cell::RefCell, cmp};
use sata_definitions::{AhciCommandHeader, AhciPortBlock};

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service, printf, println, AmcMessage,
};

use crate::{
    pci_messages::{pci_config_word_read, pci_config_word_write},
    sata_definitions::{AhciGenericHostControlBlock, HostToDeviceFIS},
};

struct AhciPortDescription {
    port_index: u8,
    port_block: &'static mut AhciPortBlock,
    command_list_region: PhysVirtPair,
    command_list: &'static mut [AhciCommandHeader],
    frame_info_struct_recv_region: PhysVirtPair,
}

impl AhciPortDescription {
    fn new(port_index: u8, port_block: &'static mut AhciPortBlock) -> Self {
        // Initialize the device
        //
        // First, print out some debug info
        println!("\tPort {}", port_index);
        println!(
            "\t\tCommand list base 0x{:08x}:{:08x}",
            port_block.command_list_base_upper, port_block.command_list_base
        );
        println!(
            "\t\tFIS base 0x{:08x}:{:08x}",
            port_block.frame_info_struct_base_upper, port_block.frame_info_struct_base
        );
        println!("\t\tSignature: 0x{:08x}", port_block.signature);
        println!("\t\tActive: 0x{:08x}", port_block.sata_active);
        println!("\t\tStatus: 0x{:08x}", port_block.sata_status);

        println!(
            "\t\tCommand and start bit? {}",
            port_block.command_and_status.view_bits::<Lsb0>()[0]
        );

        // Allocate the command-list and FIS-receive buffers which we'll give to the device
        let command_list_region = amc_alloc_physical_range(0x1000);
        let frame_info_struct_recv_region = amc_alloc_physical_range(0x1000);
        // TODO(PT): Support 64-bit physical addresses here
        // Need to use the CommandListBaseUpper / FISBaseUpper fields
        // And detect if the HW supports 64 bit addressing too
        // For now, error out if axle gives us a too-high phys addr
        assert!(
            command_list_region.phys < (u32::MAX as usize),
            "Kernel handed out a too-big address",
        );
        assert!(
            frame_info_struct_recv_region.phys < (u32::MAX as usize),
            "Kernel handed out a too-big address",
        );

        let command_list = {
            let ptr = command_list_region.virt as *mut AhciCommandHeader;
            let slice = core::ptr::slice_from_raw_parts_mut(ptr, 32);
            unsafe { &mut *slice }
        };

        /*
        let mut generic_host_control_block: &mut AhciGenericHostControlBlock =
            unsafe { &mut *(ahci_base_address as *mut AhciGenericHostControlBlock) };
            */
        let mut this = Self {
            port_index,
            port_block,
            command_list_region,
            command_list,
            frame_info_struct_recv_region,
        };

        // Now, initialize the command-list and FIS-receive buffers
        // First, request the HBA stop processing the command-list, since we're going to modify it
        this.stop_processing_command_list();
        // Wait for any ongoing access to the command list to complete
        this.wait_until_command_list_use_completes();

        // Also, request the HBA stops delivering FIS
        this.stop_receiving_frame_info_structs();
        // From the spec on FIS Receive enable bit:
        // > If software wishes to move the base, this bit must first be cleared,
        // > and software must wait for the FR bit in this register to be cleared.
        this.wait_until_fis_receive_completes();

        this.port_block.command_list_base = this.command_list_region.phys as u32;
        this.port_block.command_list_base_upper = 0;
        this.port_block.frame_info_struct_base = this.frame_info_struct_recv_region.phys as u32;
        this.port_block.frame_info_struct_base_upper = 0;

        // Ready to receive FIS
        this.start_receiving_frame_info_structs();

        /*
        for command_header in this.command_list {
            let word0 = command_header.word0.view_bits_mut::<Lsb0>();
            // Set Command FIS length (in u32 increments)
            [0..4].copy_from_slice(4);
            //
        }
        */

        this.start_processing_command_list();

        println!(
            "FIS at virt 0x{:16x}",
            this.frame_info_struct_recv_region.virt
        );

        let command_header0 = &mut this.command_list[0];
        command_header0.word0 = 0;
        let word0 = command_header0.word0.view_bits_mut::<Lsb0>();
        // Set W bit
        word0.set(6, true);
        // Set Command FIS length (in sizeof(u32) increments)
        let command_fis_len = 5;
        word0[0..4].copy_from_slice(&command_fis_len.view_bits::<Lsb0>()[0..4]);

        println!(
            "Bytecount {} before sleep",
            command_header0.phys_region_desc_byte_count
        );

        let command_table_base = amc_alloc_physical_range(0x1000);
        command_header0.command_table_desc_base = command_table_base.phys as u32;
        command_header0.command_table_desc_base_upper = 0;

        let command_fis = {
            let ptr = command_table_base.virt as *mut HostToDeviceFIS;
            unsafe { &mut *ptr }
        };
        let command_fis_as_u8 = {
            let ptr = command_table_base.virt as *mut u8;
            let slice = core::ptr::slice_from_raw_parts_mut(ptr, 5 * 4);
            unsafe { &mut *slice }
        };
        command_fis_as_u8[0] = 0x27;
        command_fis_as_u8[1] = 0b10000000;
        command_fis_as_u8[2] = 0xec;

        unsafe { libc::usleep(1000) };

        println!(
            "Bytecount {} after sleep",
            command_header0.phys_region_desc_byte_count
        );

        // Issue the command
        println!("Issuing command...");
        this.port_block
            .command_issue
            .view_bits_mut::<Lsb0>()
            .set(0, true);
        println!("Set command issue bit!");

        loop {
            /*
            println!(
                "Bytecount {} after sleep",
                command_header0.phys_region_desc_byte_count
            );
            unsafe { libc::usleep(1) };
            */
        }

        loop {}

        this
    }

    fn _set_command_and_status_bit(&mut self, bit_idx: usize, enabled: bool) {
        self.port_block
            .command_and_status
            .view_bits_mut::<Lsb0>()
            .set(bit_idx, enabled);
    }

    fn _get_command_and_status_bit(&self, bit_idx: usize) -> bool {
        self.port_block.command_and_status.view_bits::<Lsb0>()[bit_idx]
    }

    fn stop_processing_command_list(&mut self) {
        self._set_command_and_status_bit(0, false);
    }

    fn start_processing_command_list(&mut self) {
        self._set_command_and_status_bit(0, true);
    }

    fn wait_until_command_list_use_completes(&self) {
        while self._get_command_and_status_bit(15) {
            println!("Waiting for Command List Running bit to clear...");
            unsafe { libc::usleep(1) };
        }
    }

    fn stop_receiving_frame_info_structs(&mut self) {
        self._set_command_and_status_bit(4, false);
    }

    fn start_receiving_frame_info_structs(&mut self) {
        self._set_command_and_status_bit(4, true);
    }

    fn wait_until_fis_receive_completes(&self) {
        while self._get_command_and_status_bit(14) {
            println!("Waiting for FIS Receive Running bit to clear...");
            unsafe { libc::usleep(1) };
        }
    }
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
        // TODO(PT): Read the range's size from PCI
        let virt_addr = amc_map_physical_range(ahci_phys_base_address, 0x1000);
        virt_addr as *mut u8
    };
    println!("Mapped AHCI range to virt {ahci_base_address:p}");

    // The generic host control block is at the base of AHCI memory
    let mut generic_host_control_block: &mut AhciGenericHostControlBlock =
        unsafe { &mut *(ahci_base_address as *mut AhciGenericHostControlBlock) };
    println!("Got generic host control block: {generic_host_control_block:?}");

    // Detect in-use AHCI ports. From the spec:
    // > Port Implemented (PI): This register is bit significant.
    // If a bit is set to '1', the corresponding port is available for software to use.
    let mut active_ports: Vec<AhciPortDescription> = vec![];
    for bit_idx in 0..32 {
        if generic_host_control_block
            .ports_implemented
            .view_bits::<Lsb0>()[bit_idx]
            == true
        {
            println!("AHCI port {bit_idx} is implemented");

            let port_block_address = {
                // From the spec:
                // > Port offset = 100h + (PI Asserted Bit Position * 80h)
                let port_block_address_raw =
                    (ahci_base_address as usize) + 0x100 + (bit_idx * 0x80);
                port_block_address_raw as *mut u8
            };
            let mut port_block = unsafe { &mut *(port_block_address as *mut AhciPortBlock) };

            // Check whether there is a device connected to this port
            // Bits 0..3 are the Device Detection indicator
            if port_block.sata_status & 0b111 == 0x03 {
                //if port_block.sata_status.view_bits::<Lsb0>()[0..3] == [1, 0, 1] {
                println!("AHCI port {bit_idx} has a device connected to it");

                // Check whether the device is active
                // Bits 8-9 are the Interface Power Managemennt state
                if (port_block.sata_status >> 8) & 0b11 == 0x01 {
                    println!("AHCI port {bit_idx}'s device is in an active state");

                    // Is the device a SATA drive (as opposed to some other kind of device)?
                    // Ref: https://forum.osdev.org/viewtopic.php?t=37474&p=311133
                    if port_block.signature == 0x00000101 {
                        println!("AHCI port {bit_idx} has a SATA drive connected to it!");
                        // Construct a description of this port
                        active_ports.push(AhciPortDescription::new(bit_idx as u8, port_block));
                    }
                }
            }
        }
    }

    println!("Active AHCI ports:");
    for port_desc in active_ports {
        let block = port_desc.port_block;
    }

    loop {}

    0
}
