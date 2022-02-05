use std::cell::RefCell;

use crate::{gameboy::GameBoyHardwareProvider, mmu::Addressable};

pub struct SerialDebugPort {
    status: RefCell<u8>,
    data: RefCell<u8>,
}

impl SerialDebugPort {
    const SERIAL_PORT_STATUS_ADDRESS: u16 = 0xff02;
    const SERIAL_PORT_DATA_ADDRESS: u16 = 0xff01;

    pub fn new() -> Self {
        Self {
            status: RefCell::new(0),
            data: RefCell::new(0),
        }
    }

    pub fn step(&self, system: &dyn GameBoyHardwareProvider) {
        let mmu = system.get_mmu();
        if mmu.read(SerialDebugPort::SERIAL_PORT_STATUS_ADDRESS) == 0x81 {
            print!(
                "{}",
                mmu.read(SerialDebugPort::SERIAL_PORT_DATA_ADDRESS) as char
            );
            mmu.write(SerialDebugPort::SERIAL_PORT_STATUS_ADDRESS, 0x00);
        }
    }
}

impl Addressable for SerialDebugPort {
    fn contains(&self, addr: u16) -> bool {
        match addr {
            SerialDebugPort::SERIAL_PORT_STATUS_ADDRESS => true,
            SerialDebugPort::SERIAL_PORT_DATA_ADDRESS => true,
            _ => false,
        }
    }

    fn read(&self, addr: u16) -> u8 {
        match addr {
            SerialDebugPort::SERIAL_PORT_STATUS_ADDRESS => *self.status.borrow(),
            SerialDebugPort::SERIAL_PORT_DATA_ADDRESS => *self.data.borrow(),
            _ => panic!("Invalid read"),
        }
    }

    fn write(&self, addr: u16, val: u8) {
        match addr {
            SerialDebugPort::SERIAL_PORT_STATUS_ADDRESS => *self.status.borrow_mut() = val,
            SerialDebugPort::SERIAL_PORT_DATA_ADDRESS => *self.data.borrow_mut() = val,
            _ => panic!("Invalid write"),
        }
    }
}
