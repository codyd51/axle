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
        assert!(
            size.width > 0 && size.height > 0,
            "Invalid size passed to AwmCreateWindow::new({size})"
        );
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
#[cfg(target_os = "axle")]
pub struct AwmCreateWindowResponse {
    event: u32,
    pub screen_resolution: SizeU32,
    pub bytes_per_pixel: u32,
    pub framebuffer_ptr: *mut libc::c_void,
}

#[cfg(target_os = "axle")]
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

#[cfg(target_os = "axle")]
impl ExpectsEventField for AwmCreateWindowResponse {
    const EXPECTED_EVENT: u32 = AwmCreateWindow::EXPECTED_EVENT;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
#[cfg(target_os = "axle")]
pub struct AwmWindowRedrawReady {
    event: u32,
}

#[cfg(target_os = "axle")]
impl AwmWindowRedrawReady {
    pub fn new() -> Self {
        AwmWindowRedrawReady {
            event: Self::EXPECTED_EVENT,
        }
    }
}

#[cfg(target_os = "axle")]
impl ExpectsEventField for AwmWindowRedrawReady {
    const EXPECTED_EVENT: u32 = 801;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmWindowUpdateTitle {
    event: u32,
    pub title_len: u32,
    pub title: [u8; 64],
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

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmWindowResized {
    event: u32,
    pub new_size: SizeU32,
}

impl AwmWindowResized {
    pub fn new(size: Size) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            new_size: SizeU32::from(size),
        }
    }
}

impl ExpectsEventField for AwmWindowResized {
    const EXPECTED_EVENT: u32 = 808;
}
