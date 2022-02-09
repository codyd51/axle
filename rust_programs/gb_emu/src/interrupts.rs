use std::{cell::RefCell, fmt::Display};

use crate::{
    gameboy::GameBoyHardwareProvider,
    mmu::{Addressable, Mmu},
};

#[derive(Copy, Clone)]
pub enum InterruptType {
    VBlank,
    LCDStat,
    Timer,
    Serial,
    Joypad,
}

impl Display for InterruptType {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let name = match self {
            InterruptType::VBlank => "VBlank",
            InterruptType::LCDStat => "LCDStat",
            InterruptType::Timer => "Timer",
            InterruptType::Serial => "Serial",
            InterruptType::Joypad => "Joypad",
        };
        write!(f, "{}", name)
    }
}

pub struct InterruptController {
    interrupt_master_enable_flag: RefCell<bool>,
    interrupt_enable_register: RefCell<u8>,
    interrupt_flag_register: RefCell<u8>,
}

impl InterruptController {
    const INTERRUPT_ENABLE_REGISTER_ADDR: u16 = 0xffff;
    const INTERRUPT_FLAG_REGISTER_ADDR: u16 = 0xff0f;

    pub fn new() -> Self {
        Self {
            interrupt_master_enable_flag: RefCell::new(true),
            interrupt_enable_register: RefCell::new(0),
            interrupt_flag_register: RefCell::new(0),
        }
    }

    pub fn set_interrupts_globally_disabled(&self) {
        //println!("Globally disabling interrupts");
        *(self.interrupt_master_enable_flag.borrow_mut()) = false
    }

    pub fn set_interrupts_globally_enabled(&self) {
        //println!("Globally enabling interrupts");
        *(self.interrupt_master_enable_flag.borrow_mut()) = true
    }

    pub fn are_interrupts_globally_enabled(&self) -> bool {
        *(self.interrupt_master_enable_flag.borrow())
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        // If interrupts are disabled and the CPU isn't halted, we have nothing to do
        if !self.are_interrupts_globally_enabled() && !system.get_cpu().borrow().is_halted {
            return;
        }

        // Check whether we should trigger any interrupts
        let mut flags_register = self.interrupt_flag_register.borrow_mut();
        let enabled_mask_register = *(self.interrupt_enable_register.borrow());
        for bit_index in 0..4 {
            let interrupt_type = self.interrupt_type_for_bit_index(bit_index);
            // Has an interrupt been requested in the flags register?
            if *flags_register != 0 || enabled_mask_register != 0 {
                /*
                println!(
                    "Flags & Bit = val, {:02x} & {bit_index} = {}, Enabled bit {}",
                    *flags_register,
                    *flags_register & (1 << bit_index),
                    enabled_mask_register & (1 << bit_index),
                );
                println!(
                    "Flags {:04x} Enabled {:04x}",
                    *flags_register, enabled_mask_register
                );
                */
            }
            if *flags_register & (1 << bit_index) != 0 {
                // Is this interrupt enabled in the bitmask?
                if enabled_mask_register & (1 << bit_index) != 0 {
                    /*
                    println!(
                        "Dispatching interrupt {interrupt_type}! Flags before: {:08b}",
                        *flags_register
                    );
                    */

                    // If the CPU is in HALT-mode and has the IME flag disabled,
                    // just un-halt the CPU.
                    let cpu_ref = system.get_cpu();
                    if !self.are_interrupts_globally_enabled() && cpu_ref.borrow().is_halted {
                        //println!("Un-halting CPU without dispatching interrupt");
                        cpu_ref.borrow_mut().set_halted(false);
                        return;
                    }

                    // Reset this IF bit
                    *flags_register &= !(1 << bit_index);

                    //println!("\tFlags after reset: {:08b}", *flags_register);
                    // Reset the IME flag during the interrupt handler
                    self.set_interrupts_globally_disabled();

                    // Push PC to the stack
                    cpu_ref.borrow_mut().call_interrupt_vector(interrupt_type);
                } else {
                    //println!("Holding on to requested interrupt {interrupt_type} because the interrupt is disabled");
                }
            }
        }
    }

    pub fn trigger_interrupt(&self, int_type: InterruptType) {
        let bit_index = self.bit_index_for_interrupt_type(int_type);
        //println!("Requesting interrupt {int_type}, bit index {bit_index}");
        *(self.interrupt_flag_register.borrow_mut()) |= (1 << bit_index);
    }

    fn bit_index_for_interrupt_type(&self, int_type: InterruptType) -> u8 {
        match int_type {
            InterruptType::VBlank => 0,
            InterruptType::LCDStat => 1,
            InterruptType::Timer => 2,
            InterruptType::Serial => 3,
            InterruptType::Joypad => 4,
        }
    }

    fn interrupt_type_for_bit_index(&self, bit_index: u8) -> InterruptType {
        match bit_index {
            0 => InterruptType::VBlank,
            1 => InterruptType::LCDStat,
            2 => InterruptType::Timer,
            3 => InterruptType::Serial,
            4 => InterruptType::Joypad,
            _ => panic!("Invalid index"),
        }
    }
}

impl Addressable for InterruptController {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => true,
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => {
                *(self.interrupt_enable_register.borrow())
            }
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                *(self.interrupt_flag_register.borrow())
            }
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => {
                //println!("Write to interrupt enable register: {val:02x}");
                *(self.interrupt_enable_register.borrow_mut()) = val
            }
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                //println!("Write to interrupt flag register: {val:02x}");
                *(self.interrupt_flag_register.borrow_mut()) = val
            }
            _ => panic!("Unrecognised address"),
        }
    }
}
