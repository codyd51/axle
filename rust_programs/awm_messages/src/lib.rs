#![no_std]

extern crate alloc;
#[cfg(target_os = "axle")]
extern crate libc;

use agx_definitions::{Size, SizeU32};
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmCreateWindow {
    event: u32,
    pub size: SizeU32,
}

impl AwmCreateWindow {
    pub fn new(size: Size) -> Self {
        AwmCreateWindow {
            event: Self::EXPECTED_EVENT,
            size: SizeU32::from(size),
        }
    }
}

impl ExpectsEventField for AwmCreateWindow {
    const EXPECTED_EVENT: u32 = 800;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmCreateWindowResponse {
    event: u32,
    pub screen_resolution: SizeU32,
    pub bytes_per_pixel: u32,
    pub framebuffer_ptr: *mut libc::c_void,
}

impl AwmCreateWindowResponse {
    pub fn new(screen_resolution: Size, bytes_per_pixel: u32, framebuffer_ptr: usize) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            screen_resolution: SizeU32::from(screen_resolution),
            bytes_per_pixel,
            framebuffer_ptr: framebuffer_ptr as *mut libc::c_void,
        }
    }
}

impl ExpectsEventField for AwmCreateWindowResponse {
    const EXPECTED_EVENT: u32 = AwmCreateWindow::EXPECTED_EVENT;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmWindowRedrawReady {
    event: u32,
}

impl AwmWindowRedrawReady {
    pub fn new() -> Self {
        AwmWindowRedrawReady {
            event: Self::EXPECTED_EVENT,
        }
    }
}

impl ExpectsEventField for AwmWindowRedrawReady {
    const EXPECTED_EVENT: u32 = 801;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmWindowUpdateTitle {
    event: u32,
    title_len: u32,
    title: [u8; 64],
}

impl ExpectsEventField for AwmWindowUpdateTitle {
    const EXPECTED_EVENT: u32 = 813;
}

impl AwmWindowUpdateTitle {
    pub fn new(title: &str) -> Self {
        let mut title_buf = [0; 64];
        let title_len = copy_str_into_sized_slice(&mut title_buf, title);
        AwmWindowUpdateTitle {
            event: Self::EXPECTED_EVENT,
            title_len: title_len.try_into().unwrap(),
            title: title_buf,
        }
    }
}

// Close window request

#[derive(Debug, ContainsEventField)]
pub struct AwmCloseWindow {
    event: u32,
}

impl ExpectsEventField for AwmCloseWindow {
    const EXPECTED_EVENT: u32 = 814;
}
