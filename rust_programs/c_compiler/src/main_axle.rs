use alloc::{rc::Rc, string::String};
use axle_rt::{amc_message_await, amc_register_service, AmcMessage};

pub fn main() {
    amc_register_service("com.axle.c_compiler");
}
