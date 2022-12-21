#![no_std]

use core::convert::TryFrom;

pub const KB_DRIVER_SERVICE_NAME: &'static str = "com.axle.kb_driver";

// PT: Must match the definitions in the C file

#[repr(usize)]
pub enum KeyIdentifier {
    ArrowUp = 0x999,
    ArrowDown = 0x998,
    ArrowLeft = 0x997,
    ArrowRight = 0x996,
    ShiftLeft = 0x995,
    ShiftRight = 0x994,
    Escape = 0x993,
    ControlLeft = 0x992,
    CommandLeft = 0x991,
    OptionLeft = 0x990,
}

impl TryFrom<u32> for KeyIdentifier {
    type Error = ();

    fn try_from(val: u32) -> Result<KeyIdentifier, ()> {
        match val {
            0x999 => Ok(KeyIdentifier::ArrowUp),
            0x998 => Ok(KeyIdentifier::ArrowDown),
            0x997 => Ok(KeyIdentifier::ArrowLeft),
            0x996 => Ok(KeyIdentifier::ArrowRight),
            0x995 => Ok(KeyIdentifier::ShiftLeft),
            0x994 => Ok(KeyIdentifier::ShiftRight),
            0x993 => Ok(KeyIdentifier::Escape),
            0x992 => Ok(KeyIdentifier::ControlLeft),
            0x991 => Ok(KeyIdentifier::CommandLeft),
            0x990 => Ok(KeyIdentifier::OptionLeft),
            _ => Err(()),
        }
    }
}

#[repr(C)]
#[derive(Debug, PartialEq)]
pub enum KeyEventType {
    Pressed,
    Released,
}

#[repr(C)]
#[derive(Debug)]
pub struct KeyboardPacket {
    pub key: u32,
    pub event_type: KeyEventType,
}
