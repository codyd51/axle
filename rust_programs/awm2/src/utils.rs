use agx_definitions::Color;
use rand::rngs::SmallRng;
use rand::RngCore;
use rand::{Rng, SeedableRng};

#[cfg(target_os = "axle")]
pub extern crate libc;
#[cfg(target_os = "axle")]
mod conditional_imports {}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {
    pub use std::time::{SystemTime, UNIX_EPOCH};
}

use crate::utils::conditional_imports::*;

pub fn get_timestamp() -> u64 {
    #[cfg(target_os = "axle")]
    return unsafe { libc::ms_since_boot() as u64 };
    #[cfg(not(target_os = "axle"))]
    return SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;
}

pub fn random_color() -> Color {
    let seed = get_timestamp();
    let mut rng = SmallRng::seed_from_u64(seed);
    Color::new(rng.gen(), rng.gen(), rng.gen())
}

pub fn random_color_with_rng(rng: &mut SmallRng) -> Color {
    Color::new(rng.gen(), rng.gen(), rng.gen())
}
