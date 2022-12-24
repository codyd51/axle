#![no_std]

extern crate alloc;

use agx_definitions::{Rect, RectU32};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

// PT: Must match the definitions in the corresponding C header

pub const AWM_DOCK_HEIGHT: isize = 32;
pub const AWM_DOCK_SERVICE_NAME: &str = "com.axle.awm_dock";

pub trait AwmDockEvent: ExpectsEventField + ContainsEventField {}

/*
 * Sent from awm to the dock
 */

// Window created

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

impl AwmDockEvent for AwmDockWindowCreatedEvent {}

// Window title updated

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

impl AwmDockEvent for AwmDockWindowTitleUpdatedEvent {}

/*
 * Window minimized flow
 */

// Sent from awm, initiates the minimize flow

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockWindowMinimizeRequestedEvent {
    pub event: u32,
    pub window_id: u32,
}

impl ExpectsEventField for AwmDockWindowMinimizeRequestedEvent {
    const EXPECTED_EVENT: u32 = 819;
}

impl AwmDockEvent for AwmDockWindowMinimizeRequestedEvent {}

// Sent from the dock, step 2

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockWindowMinimizeWithInfo {
    event: u32,
    window_id: u32,
    task_view_frame: RectU32,
}

impl AwmDockWindowMinimizeWithInfo {
    pub fn new(window_id: u32, task_view_frame: Rect) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            window_id,
            task_view_frame: RectU32::from(task_view_frame),
        }
    }
}

impl ExpectsEventField for AwmDockWindowMinimizeWithInfo {
    const EXPECTED_EVENT: u32 = 820;
}

impl AwmDockEvent for AwmDockWindowMinimizeWithInfo {}

/*
 * Task view hovered
 */

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockTaskViewHovered {
    event: u32,
    window_id: u32,
    task_view_frame: RectU32,
}

impl AwmDockTaskViewHovered {
    pub fn new(window_id: u32, task_view_frame: Rect) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            window_id,
            task_view_frame: RectU32::from(task_view_frame),
        }
    }
}

impl ExpectsEventField for AwmDockTaskViewHovered {
    const EXPECTED_EVENT: u32 = 821;
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockTaskViewHoverExited {
    event: u32,
    window_id: u32,
}

impl AwmDockTaskViewHoverExited {
    pub fn new(window_id: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            window_id,
        }
    }
}

impl ExpectsEventField for AwmDockTaskViewHoverExited {
    const EXPECTED_EVENT: u32 = 822;
}

// Task view clicked - Unminimize window or bring to forefront

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockTaskViewClicked {
    pub event: u32,
    pub window_id: u32,
}

impl AwmDockTaskViewClicked {
    pub fn new(window_id: u32) -> Self {
        Self {
            event: Self::EXPECTED_EVENT,
            window_id,
        }
    }
}

impl ExpectsEventField for AwmDockTaskViewClicked {
    const EXPECTED_EVENT: u32 = 823;
}

impl AwmDockEvent for AwmDockTaskViewClicked {}

/*
#define AWM_DOCK_WINDOW_CLOSED 824
typedef struct awm_dock_window_closed_event {
    uint32_t event;
    uint32_t window_id;
} awm_dock_window_closed_event_t;

*/

// Window closed

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AwmDockWindowClosed {
    pub event: u32,
    pub window_id: u32,
}

impl ExpectsEventField for AwmDockWindowClosed {
    const EXPECTED_EVENT: u32 = 824;
}

impl AwmDockEvent for AwmDockWindowClosed {}