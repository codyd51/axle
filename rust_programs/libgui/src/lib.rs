#![cfg_attr(any(target_os = "axle", feature = "run_in_uefi"), no_std)]
#![feature(format_args_nl)]

extern crate alloc;

extern crate core;
#[cfg(target_os = "axle")]
extern crate libc;

pub mod bordered;
pub mod button;
pub mod font;
pub mod label;
pub mod scroll_view;
pub mod text_input_view;
pub mod text_view;
pub mod ui_elements;
pub mod view;
pub mod window_events;

pub use window_events::KeyCode;

#[cfg(target_os = "axle")]
mod window_axle;
#[cfg(target_os = "axle")]
pub use axle_rt::{print, println};
#[cfg(target_os = "axle")]
pub use window_axle::*;

#[cfg(not(any(target_os = "axle", feature = "run_in_uefi")))]
mod window_std;

#[cfg(feature = "run_in_uefi")]
mod window_uefi;
#[cfg(feature = "run_in_uefi")]
pub use window_uefi::*;

#[cfg(not(any(target_os = "axle", feature = "run_in_uefi")))]
pub use std::{print, println};
#[cfg(not(any(target_os = "axle", feature = "run_in_uefi")))]
pub use window_std::*;
