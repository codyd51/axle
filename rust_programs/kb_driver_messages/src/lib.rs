#![no_std]

#[cfg(target_os = "axle")]
use axle_rt::{amc_message_send, amc_message_send_untyped};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const KB_DRIVER_SERVICE_NAME: &'static str = "com.axle.kb_driver";

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
