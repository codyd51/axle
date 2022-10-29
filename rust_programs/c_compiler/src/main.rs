#![cfg_attr(feature = "run_in_axle", no_std)]
#![cfg_attr(feature = "run_in_axle", feature(start))]
#![cfg_attr(feature = "run_in_axle", feature(format_args_nl))]
#![cfg_attr(feature = "run_in_axle", feature(default_alloc_error_handler))]
//#![feature(trait_upcasting)]
#![feature(label_break_value)]
#![feature(extend_one)]
#![feature(assert_matches)]

extern crate alloc;

#[macro_use]
extern crate static_assertions;

mod codegen;
mod lexer;
mod optimizer;
mod parser;
mod simulator;

#[cfg(feature = "run_in_axle")]
pub use axle_rt::{print, println};
#[cfg(not(feature = "run_in_axle"))]
pub use std::{print, println};

#[cfg(feature = "run_in_axle")]
mod main_axle;
#[cfg(feature = "run_in_axle")]
#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    main_axle::main();
    0
}

#[cfg(not(feature = "run_in_axle"))]
mod main_std;

#[cfg(not(feature = "run_in_axle"))]
fn main() {
    main_std::main();
}
