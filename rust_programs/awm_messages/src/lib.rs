#![no_std]

extern crate alloc;
extern crate libc;

use agx_definitions::{Size, SizeU32};

pub const AWM_CREATE_WINDOW_REQUEST: u32 = 800;

#[repr(C)]
#[derive(Debug)]
pub struct AwmCreateWindow {
    event: u32,
    pub size: SizeU32,
}

impl AwmCreateWindow {
    pub fn new(size: &Size) -> Self {
        AwmCreateWindow {
            event: AWM_CREATE_WINDOW_REQUEST,
            //size: SizeU32::(size.width, size.height),
            size: SizeU32::from(size),
        }
    }
}

impl axle_rt::HasEventField for AwmCreateWindow {
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

// TODO(PT): Add derive
impl axle_rt::HasEventField for AwmCreateWindowResponse {
    fn event(&self) -> u32 {
        self.event
    }
}

pub const AWM_WINDOW_REDRAW_READY: u32 = 801;

#[repr(C)]
#[derive(Debug)]
pub struct AwmWindowRedrawReady {
    event: u32,
}

impl AwmWindowRedrawReady {
    pub fn new() -> Self {
        AwmWindowRedrawReady {
            event: AWM_WINDOW_REDRAW_READY,
        }
    }
}

impl axle_rt::HasEventField for AwmWindowRedrawReady {
    fn event(&self) -> u32 {
        self.event
    }
}
