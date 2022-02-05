use core::fmt;
use std::{cell::RefCell, collections::BTreeMap, fmt::Display};

use crate::{
    gameboy::GameBoyHardwareProvider,
    interrupts::InterruptType,
    mmu::{Addressable, Mmu},
};

#[derive(Debug, Eq, PartialEq, PartialOrd, Ord, Copy, Clone)]
pub enum Button {
    // Direction buttons
    Left,
    Right,
    Up,
    Down,
    // Action buttons
    A,
    B,
    Start,
    Select,
}

impl Display for Button {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match self {
            Button::Left => "Left",
            Button::Right => "Right",
            Button::Up => "Up",
            Button::Down => "Down",
            Button::A => "A",
            Button::B => "B",
            Button::Start => "Start",
            Button::Select => "Select",
        };
        write!(f, "{}", name)
    }
}

pub struct Joypad {
    left_arrow_pressed: RefCell<bool>,
    joypad_status: RefCell<u8>,
    button_states: RefCell<BTreeMap<Button, bool>>,
}

impl Joypad {
    const JOYPAD_STATUS_REG_ADDR: u16 = 0xff00;

    pub fn new() -> Self {
        let mut button_states = BTreeMap::from([
            (Button::Left, false),
            (Button::Right, false),
            (Button::Up, false),
            (Button::Down, false),
            (Button::A, false),
            (Button::B, false),
            (Button::Start, false),
            (Button::Select, false),
        ]);
        Self {
            joypad_status: RefCell::new(0),
            left_arrow_pressed: RefCell::new(false),
            button_states: RefCell::new(button_states),
        }
    }

    pub fn set_button_pressed(&self, button: Button) {
        let mut states = self.button_states.borrow_mut();
        *states.get_mut(&button).unwrap() = true
    }

    pub fn set_button_released(&self, button: Button) {
        let mut states = self.button_states.borrow_mut();
        *states.get_mut(&button).unwrap() = false
    }

    pub fn is_button_pressed(&self, button: Button) -> bool {
        let states = self.button_states.borrow_mut();
        states[&button]
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        /*
        if self.is_left_arrow_pressed() {
            //println!("Triggering joypad interrupt...");
            /*
            system
                .get_interrupt_controller()
                .trigger_interrupt(InterruptType::Joypad);
                */
        }
        */
    }
}

impl Addressable for Joypad {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            Joypad::JOYPAD_STATUS_REG_ADDR => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            Joypad::JOYPAD_STATUS_REG_ADDR => {
                let mut joypad_status = *self.joypad_status.borrow_mut();
                joypad_status |= 0b1111;

                let button_states = match joypad_status >> 4 {
                    0b10 => {
                        // Selected direction buttons
                        BTreeMap::from([
                            (Button::Right, 0),
                            (Button::Left, 1),
                            (Button::Up, 2),
                            (Button::Down, 3),
                        ])
                    }
                    0b01 => {
                        // Selected action buttons
                        BTreeMap::from([
                            (Button::A, 0),
                            (Button::B, 1),
                            (Button::Select, 2),
                            (Button::Start, 3),
                        ])
                    }
                    //_ => panic!("Invalid button state"),
                    _ => BTreeMap::new(),
                };
                for (button, bit_index) in button_states {
                    if self.is_button_pressed(button) {
                        joypad_status &= !(1 << bit_index);
                    }
                }

                joypad_status
            }
            _ => panic!("Unknown address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Joypad::JOYPAD_STATUS_REG_ADDR => {
                // Only bits 4 and 5 are writable
                let mut joypad_status = self.joypad_status.borrow_mut();
                // First, clear bits 4 and 5
                let mut new_status_val = *joypad_status;
                new_status_val &= !(0b110000);
                // Now, set bits 4 and 5 from the provided value
                new_status_val |= val & 0b110000;
                *joypad_status = new_status_val;
                //println!("Wrote {new_status_val:08b} to joypad status register");
            }
            _ => panic!("Unknown address"),
        }
    }
}
