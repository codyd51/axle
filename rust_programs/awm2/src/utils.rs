use agx_definitions::Color;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};

use dock_messages::AWM_DOCK_SERVICE_NAME;
use menu_bar_messages::AWM_MENU_BAR_SERVICE_NAME;

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

pub fn awm_service_is_dock(service_name: &str) -> bool {
    return service_name == AWM_DOCK_SERVICE_NAME;
}

pub fn awm_service_is_menu_bar(service_name: &str) -> bool {
    return service_name == AWM_MENU_BAR_SERVICE_NAME;
}
