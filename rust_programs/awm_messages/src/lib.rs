#![no_std]

extern crate alloc;
extern crate libc;

use agx_definitions::{Size, SizeU32};
use axle_rt::{ContainsEventField, ExpectsEventField};

#[repr(C)]
#[derive(Debug)]
pub struct AwmCreateWindow {
    event: u32,
    pub size: SizeU32,
}

impl AwmCreateWindow {
    pub fn new(size: &Size) -> Self {
        AwmCreateWindow {
            event: Self::EXPECTED_EVENT,
            size: SizeU32::from(size),
        }
    }
}

impl ExpectsEventField for AwmCreateWindow {
    const EXPECTED_EVENT: u32 = 800;
}

impl ContainsEventField for AwmCreateWindow {
    fn event(&self) -> u32 {
        self.event
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct AwmCreateWindowResponse {
    event: u32,
    pub screen_resolution: SizeU32,
    pub bytes_per_pixel: u32,
    pub framebuffer_ptr: *mut libc::c_void,
}

impl ExpectsEventField for AwmCreateWindowResponse {
    const EXPECTED_EVENT: u32 = AwmCreateWindow::EXPECTED_EVENT;
}

// TODO(PT): Add derive
impl ContainsEventField for AwmCreateWindowResponse {
    fn event(&self) -> u32 {
        self.event
    }
}

#[repr(C)]
#[derive(Debug)]
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

impl ContainsEventField for AwmWindowRedrawReady {
    fn event(&self) -> u32 {
        self.event
    }
}
