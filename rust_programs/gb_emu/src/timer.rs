use std::cell::RefCell;

use crate::{
    gameboy::GameBoyHardwareProvider,
    interrupts::InterruptType,
    mmu::{Addressable, Mmu},
};

pub struct Timer {
    divider: RefCell<u8>,
    counter: RefCell<u8>,
    modulo: RefCell<u8>,
    control: RefCell<u8>,

    tick_counter: RefCell<usize>,
}

impl Timer {
    const DIVIDER: u16 = 0xff04;
    const COUNTER: u16 = 0xff05;
    const MODULO: u16 = 0xff06;
    const CONTROL: u16 = 0xff07;

    pub fn new() -> Self {
        Self {
            divider: RefCell::new(0),
            counter: RefCell::new(0),
            modulo: RefCell::new(0),
            control: RefCell::new(0),
            tick_counter: RefCell::new(0),
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let new_divider_val = self.divider.borrow().wrapping_add(16);
        *self.divider.borrow_mut() = new_divider_val;
        return;
        let mut counter_mut = self.counter.borrow_mut();

        *self.tick_counter.borrow_mut() += 1;
        if *self.tick_counter.borrow() >= 64 {
            *self.tick_counter.borrow_mut() = 0;
            let (new_counter, did_overflow) = counter_mut.overflowing_add(1);
            *counter_mut = match did_overflow {
                false => new_counter,
                true => {
                    // Trigger an interrupt
                    system
                        .get_interrupt_controller()
                        .trigger_interrupt(InterruptType::Timer);
                    *self.modulo.borrow()
                }
            };
        }
    }
}

impl Addressable for Timer {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            Timer::DIVIDER => true,
            Timer::COUNTER => true,
            Timer::MODULO => true,
            Timer::CONTROL => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            Timer::DIVIDER => *self.divider.borrow(),
            Timer::COUNTER => *self.counter.borrow(),
            Timer::MODULO => *self.modulo.borrow(),
            Timer::CONTROL => *self.control.borrow(),
            _ => panic!("Unknown address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Timer::DIVIDER => {
                // Any write resets this register to 0
                *self.divider.borrow_mut() = 0;
            }
            Timer::COUNTER => *self.counter.borrow_mut() = val,
            Timer::MODULO => *self.modulo.borrow_mut() = val,
            Timer::CONTROL => *self.control.borrow_mut() = val,
            _ => panic!("Unknown address"),
        }
    }
}
