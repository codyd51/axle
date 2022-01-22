#[cfg_attr(not(target_os = "axle"), no_std)]

mod cpu;

extern crate alloc;

extern crate std;
#[cfg(not(target_os = "axle"))]
use {
    std::println,
};

use cpu::CpuState;

fn main() {
    let mut cpu = CpuState::new();
    loop {
        cpu.step();
    }
}
