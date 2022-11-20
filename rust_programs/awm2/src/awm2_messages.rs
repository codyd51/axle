#![no_std]

extern crate alloc;
#[cfg(target_os = "axle")]
extern crate libc;

use agx_definitions::{Rect, RectU32};
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

// PT: Must match the definitions in the corresponding C header

pub const AWM2_SERVICE_NAME: &str = "com.axle.awm";
