#![no_std]

extern crate alloc;

use agx_definitions::{Rect, RectU32};
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const AWM_MENU_BAR_HEIGHT: isize = 28;
pub const AWM_MENU_BAR_SERVICE_NAME: &str = "com.axle.awm_menu_bar";
