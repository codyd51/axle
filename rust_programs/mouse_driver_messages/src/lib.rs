#![no_std]

#[cfg(target_os = "axle")]
use axle_rt::{amc_message_send, amc_message_send_untyped};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const MOUSE_DRIVER_SERVICE_NAME: &'static str = "com.axle.mouse_driver";

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct MousePacket {
    pub event: u32,
    pub status: i8,
    pub rel_x: i8,
    pub rel_y: i8,
    pub rel_z: i8,
}

impl MousePacket {
    pub fn new(rel_x: i8, rel_y: i8, status: i8) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            status: status,
            rel_x,
            rel_y,
            rel_z: 0,
        }
    }
}

impl ExpectsEventField for MousePacket {
    const EXPECTED_EVENT: u32 = 1;
}
