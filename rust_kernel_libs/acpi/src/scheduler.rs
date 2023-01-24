use crate::apic::cpu_core_private_info;
use alloc::collections::BTreeMap;
use alloc::vec;
use alloc::vec::Vec;
use ffi_bindings::{cpu_id, println, TaskControlBlock};
use lazy_static::lazy_static;
use spin::Mutex;

lazy_static! {
    static ref ALL_TASKS: spin::Mutex<Vec<&'static TaskControlBlock>> = Mutex::new(vec![]);
    static ref CORES_TO_TASKS: spin::Mutex<BTreeMap<usize, Vec<&'static TaskControlBlock>>> =
        Mutex::new(BTreeMap::new());
}

#[no_mangle]
pub fn scheduler_track_task(task_raw: *const TaskControlBlock) {
    let cpu_id = unsafe { cpu_id() };
    let task_ref: &'static TaskControlBlock = unsafe { &*task_raw };

    let mut cores_to_tasks = CORES_TO_TASKS.lock();

    if !cores_to_tasks.contains_key(&cpu_id) {
        //println!("[Sched] Tracking tasks on {cpu_id}");
        cores_to_tasks.insert(cpu_id, vec![]);
    }

    //println!("[Sched] Core {cpu_id} tracking task {task_ref:?}");
    cores_to_tasks.get_mut(&cpu_id).unwrap().push(task_ref);
    ALL_TASKS.lock().push(task_ref);
}

#[no_mangle]
pub fn tasking_get_task_with_pid(pid: u32) -> *const TaskControlBlock {
    ALL_TASKS
        .lock()
        .iter()
        .find(|&&tcb| tcb.pid == pid)
        .map_or(core::ptr::null(), |&tcb| tcb)
}
