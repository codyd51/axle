use crate::mmu::Addressable;

pub struct Joypad {}

impl Joypad {
    const JOYPAD_STATUS_REG_ADDR: u16 = 0xff00;

    pub fn new() -> Self {
        Self {}
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
            // TODO(PT): This simulates all buttons not pressed
            Joypad::JOYPAD_STATUS_REG_ADDR => 0xff,
            _ => panic!("Unknown address"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            Joypad::JOYPAD_STATUS_REG_ADDR => println!("Ignoring write to joypad input selection"),
            _ => panic!("Unknown address"),
        }
    }
}
