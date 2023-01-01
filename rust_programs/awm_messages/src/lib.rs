#![no_std]

extern crate alloc;
#[cfg(target_os = "axle")]
extern crate libc;

use agx_definitions::{Color, Point, PointU32, Rect, RectU32, Size, SizeU32};
use alloc::vec::Vec;
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const AWM2_SERVICE_NAME: &str = "com.axle.awm";

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

impl AwmCloseWindow {
    pub fn new() -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
        }
    }
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

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseScrolled {
    event: u32,
    pub mouse_point: PointU32,
    pub delta_z: i8,
}

impl AwmMouseScrolled {
    pub fn new(mouse_point: Point, delta_z: i8) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            mouse_point: PointU32::from(mouse_point),
            delta_z,
        }
    }
}

impl ExpectsEventField for AwmMouseScrolled {
    const EXPECTED_EVENT: u32 = 807;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseLeftClickStarted {
    event: u32,
    pub mouse_pos: PointU32,
}

impl AwmMouseLeftClickStarted {
    pub fn new(mouse_pos: Point) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            mouse_pos: PointU32::from(mouse_pos),
        }
    }
}

impl ExpectsEventField for AwmMouseLeftClickStarted {
    const EXPECTED_EVENT: u32 = 809;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseLeftClickEnded {
    event: u32,
    mouse_pos: PointU32,
}

impl AwmMouseLeftClickEnded {
    pub fn new(mouse_pos: Point) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            mouse_pos: PointU32::from(mouse_pos),
        }
    }
}

impl ExpectsEventField for AwmMouseLeftClickEnded {
    const EXPECTED_EVENT: u32 = 811;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseEntered {
    event: u32,
}

impl AwmMouseEntered {
    pub fn new() -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
        }
    }
}

impl ExpectsEventField for AwmMouseEntered {
    const EXPECTED_EVENT: u32 = 802;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseExited {
    event: u32,
}

impl AwmMouseExited {
    pub fn new() -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
        }
    }
}

impl ExpectsEventField for AwmMouseExited {
    const EXPECTED_EVENT: u32 = 803;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseMoved {
    event: u32,
    pub mouse_pos: PointU32,
}

impl AwmMouseMoved {
    pub fn new(mouse_pos: Point) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            mouse_pos: PointU32::from(mouse_pos),
        }
    }
}

impl ExpectsEventField for AwmMouseMoved {
    const EXPECTED_EVENT: u32 = 804;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmMouseDragged {
    event: u32,
    // TOOD(PT): Update this event to send a PointU32
    mouse_x: u32,
    mouse_y: u32,
}

impl AwmMouseDragged {
    pub fn new(mouse_pos: Point) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            mouse_x: mouse_pos.x as u32,
            mouse_y: mouse_pos.y as u32,
        }
    }
}

impl ExpectsEventField for AwmMouseDragged {
    const EXPECTED_EVENT: u32 = 810;
}

// Sent from preferences to awm
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmDesktopTraitsRequest {
    event: u32,
}

impl ExpectsEventField for AwmDesktopTraitsRequest {
    const EXPECTED_EVENT: u32 = 815;
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ColorRgba {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
}

impl From<Color> for ColorRgba {
    fn from(value: Color) -> Self {
        Self {
            r: value.r,
            g: value.g,
            b: value.b,
            a: 0xff,
        }
    }
}

// Sent from awm to preferences
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct AwmDesktopTraitsResponse {
    event: u32,
    desktop_gradient_inner_color: ColorRgba,
    desktop_gradient_outer_color: ColorRgba,
}

impl AwmDesktopTraitsResponse {
    pub fn new(desktop_gradient_inner_color: Color, desktop_gradient_outer_color: Color) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            desktop_gradient_inner_color: ColorRgba::from(desktop_gradient_inner_color),
            desktop_gradient_outer_color: ColorRgba::from(desktop_gradient_outer_color),
        }
    }
}

impl ExpectsEventField for AwmDesktopTraitsResponse {
    const EXPECTED_EVENT: u32 = 815;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmWindowPartialRedraw {
    event: u32,
    pub rect_count: u32,
    pub rects: [RectU32; 64],
}

impl AwmWindowPartialRedraw {
    pub fn new(rects: &Vec<Rect>) -> Self {
        let src_rects_u32: Vec<RectU32> = rects.iter().map(|r| RectU32::from(*r)).collect();
        let mut dst_rects = [RectU32::zero(); 64];
        dst_rects[..rects.len()].copy_from_slice(&src_rects_u32);
        Self {
            event: Self::EXPECTED_EVENT,
            rect_count: rects.len() as u32,
            rects: dst_rects,
        }
    }
}

impl ExpectsEventField for AwmWindowPartialRedraw {
    const EXPECTED_EVENT: u32 = 816;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmKeyDown {
    event: u32,
    key: u32,
}

impl AwmKeyDown {
    pub fn new(key: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            key,
        }
    }
}

impl ExpectsEventField for AwmKeyDown {
    const EXPECTED_EVENT: u32 = 805;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmKeyUp {
    event: u32,
    key: u32,
}

impl AwmKeyUp {
    pub fn new(key: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            key,
        }
    }
}

impl ExpectsEventField for AwmKeyUp {
    const EXPECTED_EVENT: u32 = 806;
}
