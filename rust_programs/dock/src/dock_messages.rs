#![no_std]

extern crate alloc;
extern crate libc;

use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

/*
 * Sent from awm to the dock
 */
#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockWindowCreatedEvent {
    pub event: u32,
    pub window_id: u32,
    pub title_len: u32,
    pub title: [u8; 64],
}

impl ExpectsEventField for AwmDockWindowCreatedEvent {
    const EXPECTED_EVENT: u32 = 817;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockWindowTitleUpdatedEvent {
    pub event: u32,
    pub window_id: u32,
    pub title_len: u32,
    pub title: [u8; 64],
}

impl ExpectsEventField for AwmDockWindowTitleUpdatedEvent {
    const EXPECTED_EVENT: u32 = 818;
}
