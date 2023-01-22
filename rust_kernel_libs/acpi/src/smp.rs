use crate::apic::{
    cpu_core_private_info, io_apic_mask_line, local_apic_enable, local_apic_timer_calibrate,
    local_apic_timer_start,
};
use crate::SmpInfo;
use ffi_bindings::{println, task_die};

extern "C" {
    fn smp_info_get() -> *mut SmpInfo;
}

pub fn smp_info_ref() -> &'static SmpInfo {
    unsafe { &*(smp_info_get()) }
}

#[no_mangle]
pub fn smp_core_continue() {
    // From here, the BSP and APs are on a shared code path
    let smp_info = smp_info_ref();

    println!(
        "smp_core_entry(CPU[{}])",
        cpu_core_private_info().processor_id
    );
    println!("Enabling LAPIC...");
    local_apic_enable();
    println!(
        "Enabling LAPIC timer (with int vector {})...",
        smp_info.local_apic_timer_int_vector
    );
    local_apic_timer_calibrate();
    local_apic_timer_start(5);
    //io_apic_mask_line(2);
    // TODO(PT): Once all cores have calibrated using the PIT, mask the PIT

    // Bootstrapping complete - kill this process
    println!("Bootstrap task will exit");
    unsafe {
        task_die(0);
    }
    assert!(false, "task_die should have stopped execution");
}
