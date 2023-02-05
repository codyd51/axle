use agx_definitions::{Color, Point, Rect, Size};
use alloc::{rc::Rc, string::String};
use axle_rt::amc_register_service;
use axle_rt::println;

pub fn main() {
    println!("Running in axle!");
    amc_register_service("com.axle.ttf_viewer");
    loop {}
}
