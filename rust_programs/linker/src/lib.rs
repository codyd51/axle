#![cfg_attr(feature = "run_in_axle", no_std)]
#![cfg_attr(feature = "run_in_axle", feature(start))]
#![cfg_attr(feature = "run_in_axle", feature(format_args_nl))]
#![cfg_attr(feature = "run_in_axle", feature(default_alloc_error_handler))]
#![feature(trait_upcasting)]
#![feature(label_break_value)]
#![feature(extend_one)]

extern crate alloc;

#[cfg(feature = "run_in_axle")]
pub use axle_rt::{print, println};
#[cfg(not(feature = "run_in_axle"))]
pub use std::{print, println};

mod assembly_lexer;
pub mod assembly_packer;
mod assembly_parser;
pub mod new_try;
mod records;
mod symbols;

pub use crate::new_try::{render_elf, FileLayout};
