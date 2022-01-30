use std::cell::RefCell;

use crate::mmu::Addressable;

pub struct InterruptController {
    interrupt_master_enable_flag: RefCell<bool>,
}

impl InterruptController {
    const INTERRUPT_ENABLE_REGISTER_ADDR: u16 = 0xffff;
    const INTERRUPT_FLAG_REGISTER_ADDR: u16 = 0xff0f;

    pub fn new() -> Self {
        Self {
            interrupt_master_enable_flag: RefCell::new(true),
        }
    }

    pub fn set_interrupts_globally_disabled(&self) {
        *(self.interrupt_master_enable_flag.borrow_mut()) = false
    }

    pub fn set_interrupts_globally_enabled(&self) {
        *(self.interrupt_master_enable_flag.borrow_mut()) = true
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
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => todo!(),
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                todo!()
            }
            _ => panic!("Unrecognised address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            InterruptController::INTERRUPT_ENABLE_REGISTER_ADDR => {
                println!("Write to interrupt enable register: {val:02x}");
            }
            InterruptController::INTERRUPT_FLAG_REGISTER_ADDR => {
                println!("Write to interrupt flag register: {val:02x}");
            }
            _ => panic!("Unrecognised address"),
        }
    }
}
