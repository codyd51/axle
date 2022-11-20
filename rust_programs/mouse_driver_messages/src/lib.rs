#![no_std]

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

impl ExpectsEventField for MousePacket {
    const EXPECTED_EVENT: u32 = 1;
}
