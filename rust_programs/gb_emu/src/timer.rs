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

    timer_counter_enabled: RefCell<bool>,
    counter_tick_divisor: RefCell<usize>,
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
            // TODO(PT): Should this be true by default?
            timer_counter_enabled: RefCell::new(false),
            counter_tick_divisor: RefCell::new(0),
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mut ticks = self.tick_counter.borrow_mut();
        *ticks += 1;

        // The divider should increment once every 256 clock ticks
        // We clock at 1MHz, so divide by 4
        if *ticks % 64 == 0 {
            let mut divider = self.divider.borrow_mut();
            *divider = divider.wrapping_add(1);
        }

        // The counter should only increment if it's enabled
        if *self.timer_counter_enabled.borrow() {
            // And the counter should increment at the frequency specified by the
            // control register
            if *ticks % *self.counter_tick_divisor.borrow() == 0 {
                let mut counter = self.counter.borrow_mut();
                let (new_counter, did_overflow) = counter.overflowing_add(1);
                *counter = new_counter;
                // When the counter overflows, it should be reset to the value
                // specified by the modulo register
                if did_overflow {
                    // When the counter overflows, it should be reset to the value
                    *counter = *self.modulo.borrow();
                    // And, when the counter overflows, an interrupt should trigger
                    system
                        .get_interrupt_controller()
                        .trigger_interrupt(InterruptType::Timer);
                }
            }
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
            Timer::CONTROL => {
                // Bit 2 is the timer_counter_enable
                *self.timer_counter_enabled.borrow_mut() = (val >> 2) & 0b1 == 1;
                // Divide by 4 because we run at 1MHz and the numbers are for 4MHz
                *self.counter_tick_divisor.borrow_mut() = match val & 0b11 {
                    0b00 => 1024 / 4,
                    0b01 => 16 / 4,
                    0b10 => 64 / 4,
                    0b11 => 256 / 4,
                    _ => panic!("Invalid value!"),
                };
            }
            _ => panic!("Unknown address"),
        }
    }
}
