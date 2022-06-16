#![cfg_attr(not(feature = "use_std"), no_std)]
#![cfg_attr(feature = "run_in_axle", feature(start))]
#![cfg_attr(feature = "run_in_axle", feature(format_args_nl))]
#![cfg_attr(feature = "run_in_axle", feature(default_alloc_error_handler))]
extern crate alloc;

mod cpu;
mod gameboy;
mod interrupts;
mod joypad;
mod mmu;
mod ppu;
mod serial;
mod timer;

#[cfg(not(feature = "use_std"))]
mod main_axle;
#[cfg(not(feature = "use_std"))]
#[start]
#[allow(unreachable_code)]
fn start(_argc: isize, _argv: *const *const u8) -> isize {
    main_axle::main();
    0
}

#[cfg(feature = "use_std")]
mod main_std;
#[cfg(feature = "use_std")]
fn main() {
    main_std::main();
}
