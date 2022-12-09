use agx_definitions::{PointU32, SizeU32};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub trait AwmWindowEvent: ExpectsEventField + ContainsEventField {}

#[derive(Debug, Copy, Clone)]
pub struct KeyCode(pub u32);

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct KeyDown {
    event: u32,
    pub key: u32,
}

impl ExpectsEventField for KeyDown {
    const EXPECTED_EVENT: u32 = 805;
}

impl AwmWindowEvent for KeyDown {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct KeyUp {
    event: u32,
    pub key: u32,
}

impl ExpectsEventField for KeyUp {
    const EXPECTED_EVENT: u32 = 806;
}

impl AwmWindowEvent for KeyUp {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseMoved {
    event: u32,
    pub mouse_pos: PointU32,
}

impl ExpectsEventField for MouseMoved {
    const EXPECTED_EVENT: u32 = 804;
}

impl AwmWindowEvent for MouseMoved {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseDragged {
    event: u32,
    mouse_pos: PointU32,
}

impl ExpectsEventField for MouseDragged {
    const EXPECTED_EVENT: u32 = 810;
}

impl AwmWindowEvent for MouseDragged {}

// TODO(PT): Replace with awm_messages version
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseLeftClickStarted {
    event: u32,
    pub mouse_pos: PointU32,
}

impl ExpectsEventField for MouseLeftClickStarted {
    const EXPECTED_EVENT: u32 = 809;
}

impl AwmWindowEvent for MouseLeftClickStarted {}

// TODO(PT): Replace with awm_messages version
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseLeftClickEnded {
    event: u32,
    mouse_pos: PointU32,
}

impl ExpectsEventField for MouseLeftClickEnded {
    const EXPECTED_EVENT: u32 = 811;
}

impl AwmWindowEvent for MouseLeftClickEnded {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseEntered {
    event: u32,
}

impl ExpectsEventField for MouseEntered {
    const EXPECTED_EVENT: u32 = 802;
}

impl AwmWindowEvent for MouseEntered {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseExited {
    event: u32,
}

impl ExpectsEventField for MouseExited {
    const EXPECTED_EVENT: u32 = 803;
}

impl AwmWindowEvent for MouseExited {}

// TODO(PT): Drop this in favor of the version in awm_messages
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct MouseScrolled {
    event: u32,
    pub mouse_point: PointU32,
    pub delta_z: i8,
}

impl ExpectsEventField for MouseScrolled {
    const EXPECTED_EVENT: u32 = 807;
}

impl AwmWindowEvent for MouseScrolled {}

// TODO(PT): Drop this in favor of the version in awm_messages
#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct WindowResized {
    event: u32,
    pub new_size: SizeU32,
}

impl ExpectsEventField for WindowResized {
    const EXPECTED_EVENT: u32 = 808;
}

impl AwmWindowEvent for WindowResized {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct WindowResizeEnded {
    event: u32,
}

impl ExpectsEventField for WindowResizeEnded {
    const EXPECTED_EVENT: u32 = 816;
}

impl AwmWindowEvent for WindowResizeEnded {}

#[repr(C)]
#[derive(Debug, Clone, Copy, ContainsEventField)]
pub struct WindowCloseRequested {
    event: u32,
}

impl ExpectsEventField for WindowCloseRequested {
    const EXPECTED_EVENT: u32 = 814;
}

impl AwmWindowEvent for WindowCloseRequested {}
