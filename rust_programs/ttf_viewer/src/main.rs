#![cfg_attr(target_os = "axle", no_std)]
#![cfg_attr(target_os = "axle", feature(start))]
#![cfg_attr(target_os = "axle", feature(format_args_nl))]
#![cfg_attr(target_os = "axle", feature(default_alloc_error_handler))]

mod font_viewer;
mod utils;

#[cfg(target_os = "axle")]
mod main_axle;
#[cfg(target_os = "axle")]
extern crate alloc;

#[cfg(not(target_os = "axle"))]
mod main_std;

#[cfg(target_os = "axle")]
mod run_in_axle {
    use crate::main_axle;
    pub use axle_rt::{print, println};

    #[start]
    #[allow(unreachable_code)]
    fn start(_argc: isize, _argv: *const *const u8) -> isize {
        main_axle::main();
        0
    }
}

#[cfg(not(target_os = "axle"))]
pub use std::{print, println};
#[cfg(not(target_os = "axle"))]
fn main() {
    main_std::main();
}
