#![no_std]

#[cfg(target_os = "axle")]
use axle_rt::{amc_message_send, amc_message_send_untyped};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const PREFERENCES_SERVICE_NAME: &'static str = "com.axle.preferences";
