#![no_std]
#![feature(start)]
#![feature(raw_ref_op)]
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
    adi_event_await, adi_register_driver, adi_send_eoi, amc_has_message, amc_message_await_untyped,
    core_commands::{
        amc_alloc_physical_range, amc_map_physical_range, AmcMapPhysicalRangeRequest,
        PhysRangeMapping, PhysVirtPair,
    },
    print, ContainsEventField, ExpectsEventField,
};
use axle_rt_derive::ContainsEventField;
use core::{cell::RefCell, cmp, mem};
use sata_definitions::{AhciCommandHeader, AhciPortBlock, RawPhysRegionDescriptor};

use axle_rt::{
    amc_message_await, amc_message_send, amc_register_service, printf, println, AmcMessage,
};

use crate::{
    pci_messages::{pci_config_word_read, pci_config_word_write, AHCI_INTERRUPT_VECTOR},
    sata_definitions::{
        AhciCommandHeaderWord0, AhciCommandHeaderWord0Bits2, AhciGenericHostControlBlock,
        CommandOpcode, HostToDeviceFIS, IdentifyDeviceData,
    },
};

#[derive(Debug)]
struct CommandRequest {
    opcode: CommandOpcode,
    is_write: bool,
    sector_count: u32,
}

impl CommandRequest {
    fn new_read_command() -> Self {
        Self {
            opcode: CommandOpcode::ReadDmaExt,
            is_write: false,
            sector_count: 1,
        }
    }

    fn new_write_command() -> Self {
        Self {
            opcode: CommandOpcode::WriteDmaExt,
            is_write: true,
            sector_count: 1,
        }
    }

    fn new_identify_command() -> Self {
        Self {
            opcode: CommandOpcode::IdentifyDevice,
            is_write: false,
            sector_count: 1,
        }
    }
}

#[derive(Debug)]
struct ActiveCommand {
    command_slot: usize,
    command_type: CommandOpcode,
    command_table_buf: PhysRangeMapping,
    phys_region_descriptors: Vec<PhysRegionDescriptor>,
}

impl ActiveCommand {
    fn new(command_slot: usize, command_type: CommandOpcode) -> Self {
        let command_table_buf = amc_alloc_physical_range(0x1000);
        Self {
            command_slot,
            command_type,
            command_table_buf,
            phys_region_descriptors: vec![],
        }
    }

    fn complete(&self) {
        match self.command_type {
            CommandOpcode::ReadDmaExt => {
                println!("Read DMA ext completed from drive");
                let sector_data = {
                    let phys_region = &self.phys_region_descriptors[0].phys_region_buf;
                    let region_base = phys_region.addr.virt;
                    let slice =
                        core::ptr::slice_from_raw_parts(region_base as *const u8, phys_region.size);
                    unsafe { &*(slice as *const [u8]) }
                };
                let chunk_size = 64;
                for (offset, line) in sector_data.chunks(chunk_size).enumerate() {
                    print!("{:04x}: ", offset * chunk_size);
                    // TODO(PT): Fixup endianness?
                    for word in line.chunks(4) {
                        for byte in word {
                            print!("{}", *byte as char);
                        }
                        print!(" ");
                    }
                    println!();
                }
            }
            CommandOpcode::WriteDmaExt => {
                println!("Write DMA ext completed from drive");
            }
            CommandOpcode::IdentifyDevice => {
                println!("Interpreting results of IDENTIFY DEVICE...");
                let identify_block = {
                    let region = &self.phys_region_descriptors[0].phys_region_buf;
                    let ptr = region.addr.virt;
                    unsafe { &*(ptr as *const IdentifyDeviceData) }
                };
                println!("Drive serial number: {:?}", &identify_block.serial_number());
            }
        }
    }
}

#[derive(Debug)]
pub struct PhysRegionDescriptor {
    raw_descriptor: &'static mut RawPhysRegionDescriptor,
    pub phys_region_buf: PhysRangeMapping,
}

impl PhysRegionDescriptor {
    pub fn from_virt_addr(addr: usize) -> Self {
        let ptr = addr as *mut RawPhysRegionDescriptor;
        let raw_desc = unsafe { &mut *ptr };

        raw_desc.set_interrupt_on_completion(true);

        let phys_region_buf_size = 0x1000;
        println!("Allocating PhysRegionDescriptor buf");
        let phys_region_buf = amc_alloc_physical_range(phys_region_buf_size);

        raw_desc.set_byte_count(phys_region_buf_size as u32);
        raw_desc.set_data_base_address(phys_region_buf.addr.phys as u64);
        println!("Phys region data base: {:016x}", phys_region_buf.addr.phys);

        PhysRegionDescriptor {
            raw_descriptor: raw_desc,
            phys_region_buf,
        }
    }
}

struct AhciPortDescription {
    port_index: u8,
    port_block: &'static mut AhciPortBlock,
    command_list_region: PhysRangeMapping,
    command_list: &'static mut [AhciCommandHeader],
    frame_info_struct_recv_region: PhysRangeMapping,

    active_commands: Vec<ActiveCommand>,
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
            command_list_region.addr.phys < (u32::MAX as usize),
            "Kernel handed out a too-big address",
        );
        assert!(
            frame_info_struct_recv_region.addr.phys < (u32::MAX as usize),
            "Kernel handed out a too-big address",
        );

        let command_list = {
            let ptr = command_list_region.addr.virt as *mut AhciCommandHeader;
            let slice = core::ptr::slice_from_raw_parts_mut(ptr, 32);
            unsafe { &mut *slice }
        };

        let mut this = Self {
            port_index,
            port_block,
            command_list_region,
            command_list,
            frame_info_struct_recv_region,
            active_commands: vec![],
        };

        // Now, initialize the command-list and FIS-receive buffers
        // First, request the HBA stop processing the command-list, since we're going to modify it
        this.stop_processing_command_list();
        // Also, request the HBA stops delivering FIS
        this.stop_receiving_frame_info_structs();

        // Wait for any ongoing access to the command list to complete
        this.wait_until_command_list_use_completes();
        // From the spec on FIS Receive enable bit:
        // > If software wishes to move the base, this bit must first be cleared,
        // > and software must wait for the FR bit in this register to be cleared.
        this.wait_until_fis_receive_completes();

        this.port_block.command_list_base = this.command_list_region.addr.phys as u32;
        this.port_block.command_list_base_upper = 0;
        this.port_block.frame_info_struct_base =
            this.frame_info_struct_recv_region.addr.phys as u32;
        this.port_block.frame_info_struct_base_upper = 0;

        println!(
            "FIS at virt 0x{:16x}",
            this.frame_info_struct_recv_region.addr.virt
        );

        println!(
            "Port interrupt status: {}",
            this.port_block.interrupt_status
        );

        // Clear pending interrupts with a write-clear
        this.port_block.interrupt_status = 0xffffffff;

        // Wait for any ongoing access to the command list to complete
        this.wait_until_command_list_use_completes();

        // Ready to receive FIS
        // 10.3.2: FIS enable must be before CL enable
        this.start_receiving_frame_info_structs();
        this.start_processing_command_list();

        println!(
            "End of port init interrupt status: {}",
            this.port_block.interrupt_status
        );

        this
    }

    fn find_free_command_slot(&self) -> usize {
        for (bit_idx, bit) in self
            .port_block
            .command_issue
            .view_bits::<Lsb0>()
            .iter()
            .enumerate()
        {
            if *bit == false {
                // Found a free command slot!
                return bit_idx;
            }
        }
        // TODO(PT): Option<CommandSlot> instead of panic on failure?
        panic!("Failed to find a free command slot!");
    }

    fn send_command_req(&mut self, cmd_request: &CommandRequest) {
        let free_command_slot_idx = self.find_free_command_slot();
        let command_header = &mut self.command_list[free_command_slot_idx];
        let mut active_command = ActiveCommand::new(free_command_slot_idx, cmd_request.opcode);
        command_header
            .set_command_table_desc_base(active_command.command_table_buf.addr.phys as u64);

        let mut word0 = &mut command_header.word0;
        // Command FIS length (in sizeof(u32) increments)
        assert_eq!(mem::size_of::<HostToDeviceFIS>() / mem::size_of::<u32>(), 5);
        word0.set_command_fis_len(
            (mem::size_of::<HostToDeviceFIS>() / mem::size_of::<u32>()) as u32,
        );
        word0.set_write(cmd_request.is_write);
        word0.set_clear_busy_upon_r_ok(true);

        let phys_region_count = 1;
        word0.set_phys_region_desc_table_len(phys_region_count as u32);

        for region_idx in 0..phys_region_count {
            let phys_region_descriptor = PhysRegionDescriptor::from_virt_addr(
                active_command.command_table_buf.addr.virt
                    + 0x80
                    + (mem::size_of::<PhysRegionDescriptor>() * region_idx),
            );
            active_command
                .phys_region_descriptors
                .push(phys_region_descriptor);
        }

        let h2d_fis = HostToDeviceFIS::from_virt_addr(active_command.command_table_buf.addr.virt);
        h2d_fis.set_command(active_command.command_type);
        h2d_fis.set_is_command(true);
        h2d_fis.set_sector_count(1);

        self.active_commands.push(active_command);

        // Issue the command
        println!("Issuing command...");
        self.port_block.command_issue |= (1 << free_command_slot_idx);
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

    fn handle_interrupt(&mut self) {
        let port_interrupt_status = self.port_block.interrupt_status;
        println!("Device interrupt status: {:032b}", port_interrupt_status);
        self.port_block.clear_interrupt_status_mask();

        // Any error bits set?
        let error_bits = [04, 23, 24, 26, 27, 28, 29, 30];
        for error_bit in error_bits {
            if *port_interrupt_status
                .view_bits::<Lsb0>()
                .get(error_bit)
                .unwrap()
            {
                todo!("Handle port error");
            }
        }

        self.active_commands.retain(|active_cmd| {
            // Is the associated 'command running' bit now unset?
            let active_command_completed = self
                .port_block
                .command_issue
                .view_bits::<Lsb0>()
                .get(active_cmd.command_slot)
                .unwrap()
                == false;

            active_cmd.complete();

            !active_command_completed
        });
    }
}

fn handle_interrupt(
    generic_host_control_block: &mut AhciGenericHostControlBlock,
    active_ports: &mut BTreeMap<usize, AhciPortDescription>,
) {
    println!("AHCI interrupt");

    // Ports with an interrupt to service will have a corresponding bit set in the IS register
    let port_indexes_with_interrupt: Vec<usize> = generic_host_control_block
        .interrupt_status
        .view_bits::<Lsb0>()
        .iter_ones()
        .collect();

    // Now that we've stored the ports with interrupts to service,
    // clear the top-level interrupts-to-service mask.
    generic_host_control_block.clear_interrupt_status_mask();

    for port_idx_with_interrupt in &port_indexes_with_interrupt {
        let port_desc_with_interrupt = active_ports.get_mut(&port_idx_with_interrupt).unwrap();
        port_desc_with_interrupt.handle_interrupt();
    }

    adi_send_eoi(AHCI_INTERRUPT_VECTOR);
}

#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    amc_register_service("com.axle.sata_driver");
    adi_register_driver("com.axle.sata_driver", AHCI_INTERRUPT_VECTOR);

    println!("SATA driver running!");

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

    println!(
        "Interrupt status: 0x{:08x}",
        generic_host_control_block.interrupt_status
    );

    // AHCI enable bit (as opposed to legacy communication)
    generic_host_control_block.global_host_control.set(31, true);

    // Detect in-use AHCI ports. From the spec:
    // > Port Implemented (PI): This register is bit significant.
    // If a bit is set to '1', the corresponding port is available for software to use.
    let mut active_ports: BTreeMap<usize, AhciPortDescription> = BTreeMap::new();
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
                println!("AHCI port {bit_idx} has a device connected to it");

                // Check whether the device is active
                // Bits 8-9 are the Interface Power Management state
                if (port_block.sata_status >> 8) & 0b11 == 0x01 {
                    println!("AHCI port {bit_idx}'s device is in an active state");

                    // Is the device a SATA drive (as opposed to some other kind of device)?
                    // Ref: https://forum.osdev.org/viewtopic.php?t=37474&p=311133
                    if port_block.signature == 0x00000101 {
                        println!("AHCI port {bit_idx} has a SATA drive connected to it!");
                        //port_block.interrupt_status = 1;
                        println!(
                            "Device interrupt status: {:032b} {:08x}",
                            port_block.interrupt_status, port_block.interrupt_status
                        );
                        // Write-clear active interrupts
                        // Construct a description of this port
                        active_ports
                            .insert(bit_idx, AhciPortDescription::new(bit_idx as u8, port_block));
                    }
                }
            }
        }
    }

    // Clear pending interrupts for in the top-level port status
    generic_host_control_block.interrupt_status = 0xffffffff;

    // Enable interrupts in each port
    for (_, port) in &mut active_ports {
        port.port_block.interrupt_enable = 0xffffffff;
    }

    // Set global Interrupt Enable bit
    // The spec says this must be done after clearing the IS field in each port
    // 10.1.2: System software must always ensure that the PxIS (clear this first)
    //  and IS.IPS (clear this second) registers are cleared to '0' before programming
    //  the PxIE and GHC.IE registers. This will prevent any residual bits set in these
    //  registers from causing an interrupt to be asserted.
    generic_host_control_block.global_host_control.set(1, true);

    loop {
        let awoke_for_interrupt = adi_event_await(AHCI_INTERRUPT_VECTOR);
        if awoke_for_interrupt {
            handle_interrupt(generic_host_control_block, &mut active_ports);
        } else {
            while amc_has_message(None) {
                println!("Consuming message...");
                unsafe {
                    amc_message_await_untyped(None);
                }
            }
            println!("Sending AHCI command FIS...");
            let port_desc = active_ports.get_mut(&0).unwrap();
            port_desc.send_command_req(&CommandRequest::new_read_command());
        }
    }

    0
}
