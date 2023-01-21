use crate::apic::{
    cpu_core_private_info, local_apic_enable, local_apic_timer_calibrate, local_apic_timer_start,
};
use crate::SmpInfo;
use ffi_bindings::println;

extern "C" {
    fn smp_info_get() -> *mut SmpInfo;
}

pub fn smp_info_ref() -> &'static SmpInfo {
    unsafe { &*(smp_info_get()) }
}

