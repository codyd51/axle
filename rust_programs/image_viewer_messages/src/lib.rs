#![no_std]

extern crate alloc;
use core::{
    alloc::Layout,
    intrinsics::copy_nonoverlapping,
    mem::{align_of, size_of},
};

use alloc::alloc::alloc;
use alloc::vec::Vec;
#[cfg(target_os = "axle")]
use axle_rt::{amc_message_send, amc_message_send_untyped};
use axle_rt::{copy_str_into_sized_slice, printf, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const IMAGE_VIEWER_SERVICE_NAME: &'static str = "com.axle.image_viewer";

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct LoadImage {
    pub event: u32,
    pub path: [u8; 128],
}

impl ExpectsEventField for LoadImage {
    const EXPECTED_EVENT: u32 = 1;
}

impl LoadImage {
    pub fn send(path: &str) {
        let mut msg = LoadImage {
            event: Self::EXPECTED_EVENT,
            path: [0; 128],
        };
        copy_str_into_sized_slice(&mut msg.path, path);
        printf!("Sending message to {}\n...", IMAGE_VIEWER_SERVICE_NAME);
        amc_message_send(IMAGE_VIEWER_SERVICE_NAME, msg);
    }
}
