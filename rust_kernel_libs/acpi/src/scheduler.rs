use crate::amc::amc_service_inbox_len;
use crate::apic::cpu_core_private_info;
use alloc::alloc::alloc;
use alloc::collections::BTreeMap;
use alloc::vec;
use alloc::vec::Vec;
use core::alloc::Layout;
use core::cmp::min;
use core::mem::align_of;
use ffi_bindings::{
    amc_core_populate_task_info_int, amc_service_of_task, cpu_id, getpid, println,
    vas_get_active_state, vas_is_page_present, vas_load_state, TaskContext, TaskControlBlock,
    TaskViewerGetTaskInfoResponse, TaskViewerTaskInfo, VasRange,
};
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
pub fn scheduler_drop_task(task_raw: *const TaskControlBlock) {
    let task_ref: &'static TaskControlBlock = unsafe { &*task_raw };

    ALL_TASKS.lock().retain(|&t| !core::ptr::eq(t, task_ref));
    let mut cores_to_tasks = CORES_TO_TASKS.lock();
    for (_core_id, task_list) in cores_to_tasks.iter_mut() {
        task_list.retain(|&t| !core::ptr::eq(t, task_ref));
    }
    core::mem::forget(task_ref);
}

#[no_mangle]
pub fn tasking_get_task_with_pid(pid: u32) -> *const TaskControlBlock {
    ALL_TASKS
        .lock()
        .iter()
        .find(|&&tcb| tcb.pid == pid)
        .map_or(core::ptr::null(), |&tcb| tcb)
}

#[no_mangle]
pub unsafe fn tasking_populate_tasks_info() -> *mut TaskViewerGetTaskInfoResponse {
    let all_tasks = ALL_TASKS.lock();
    let tasks_count = all_tasks.len();

    let tasks_info = TaskViewerGetTaskInfoResponse::new(tasks_count);
    let tasks = unsafe {
        let tasks_slice =
            core::ptr::slice_from_raw_parts(core::ptr::addr_of!((*tasks_info).tasks), tasks_count);
        let tasks_slice: &mut [TaskViewerTaskInfo] =
            &mut *(tasks_slice as *mut [TaskViewerTaskInfo]);
        tasks_slice
    };

    for (i, &task) in all_tasks.iter().enumerate() {
        let name_slice = {
            let name_slice = core::ptr::slice_from_raw_parts(task.name as *const u8, 64);
            let name_slice: &[u8] = &*(name_slice as *const [u8]);
            name_slice
        };
        tasks[i].name.copy_from_slice(name_slice);
        tasks[i].pid = task.pid;

        let machine_state = &*(task.machine_state as *const TaskContext);
        tasks[i].rip = machine_state.rip;

        tasks[i].user_mode_rip = if task.pid != getpid() as u32 {
            let current_vas = vas_get_active_state();
            // Load the VAS state so we can access the relevant memory
            vas_load_state(task.vas_state);
            let user_mode_rip = find_user_mode_rip((*task.machine_state).rbp);
            vas_load_state(current_vas);
            user_mode_rip
        } else {
            find_user_mode_rip((*task.machine_state).rbp)
        };

        let vas_ranges_to_copy = min(16, (*task.vas_state).range_count) as usize;
        tasks[i].vas_range_count = vas_ranges_to_copy as _;
        let vas_ranges_slice = {
            let vas_ranges_slice = core::ptr::slice_from_raw_parts(
                core::ptr::addr_of!((*task.vas_state).ranges),
                (*task.vas_state).range_count as usize,
            );
            let vas_ranges_slice: &[VasRange] = &*(vas_ranges_slice as *const [VasRange]);
            vas_ranges_slice
        };
        for j in 0..vas_ranges_to_copy {
            tasks[i].vas_ranges[j].start = vas_ranges_slice[j].start;
            tasks[i].vas_ranges[j].size = vas_ranges_slice[j].size;
        }

        // Copy AMC service info
        let amc_service_of_task = amc_service_of_task(task);
        let has_amc_service = amc_service_of_task != core::ptr::null();
        tasks[i].has_amc_service = has_amc_service;
        if has_amc_service {
            tasks[i].pending_amc_messages = amc_service_inbox_len(amc_service_of_task) as _;
        }
    }

    tasks_info
}

unsafe fn find_user_mode_rip(rbp: u64) -> u64 {
    let mut rbp = rbp;
    let mut user_mode_rip = 0;
    for _ in 0..16 {
        if rbp < 0xFFFF900000000000 && !vas_is_page_present(vas_get_active_state(), rbp) {
            println!("rbp {rbp:016x} is unmapped");
            // VAS is not in the kernel heap and is unmapped
            break;
        }
        let rbp_slice = {
            let rbp_slice = core::ptr::slice_from_raw_parts(core::ptr::addr_of!(rbp), 2);
            let rbp_slice: &[u64] = &*(rbp_slice as *const [u64]);
            rbp_slice
        };
        let rip = rbp_slice[1];

        // Is the RIP in the canonical lower-half? If so, it's probably a user-mode return address
        if rip & (1 << 63_usize) == 0 {
            user_mode_rip = rip;
            break;
        }
        rbp = rbp_slice[0];
    }
    println!("Found user-mode RIP {user_mode_rip:016x}");
    user_mode_rip
}
