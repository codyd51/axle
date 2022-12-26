#![no_std]

use agx_definitions::Color;
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const PREFERENCES_SERVICE_NAME: &'static str = "com.axle.preferences";

#[derive(Debug, Copy, Clone)]
pub struct ColorRGBA {
    pub vals: [u8; 4],
}

impl From<ColorRGBA> for Color {
    fn from(c: ColorRGBA) -> Self {
        Color::new(c.vals[0], c.vals[1], c.vals[2])
    }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct PreferencesUpdated {
    event: u32,
    pub from: ColorRGBA,
    pub to: ColorRGBA,
}

impl ExpectsEventField for PreferencesUpdated {
    const EXPECTED_EVENT: u32 = 812;
}
