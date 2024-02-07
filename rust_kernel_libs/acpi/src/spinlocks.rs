use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use core::hint;
use core::sync::atomic::{AtomicBool, Ordering};
use ffi_bindings::Spinlock;
use lazy_static::lazy_static;
use spin::Mutex;

lazy_static! {
    // TODO(PT): Eventually, we could replace the C representation with an FFI-safe Rust-managed version entirely
    static ref SPINLOCKS: spin::Mutex<BTreeMap<&'static Spinlock, AtomicBool>> = Mutex::new(BTreeMap::new());
}

unsafe fn track_lock_if_necessary(spinlock: &'static Spinlock) {
    let mut lock_ptrs_to_locks = SPINLOCKS.lock();
    if !lock_ptrs_to_locks.contains_key(&spinlock) {
        lock_ptrs_to_locks.insert(spinlock, AtomicBool::new(false));
    }
}

#[no_mangle]
pub unsafe fn spinlock_acquire_spin(spinlock_raw: *const Spinlock) {
    let spinlock: &'static Spinlock = &*spinlock_raw;
    track_lock_if_necessary(spinlock);
    let spinlocks = SPINLOCKS.lock();
    let is_locked = spinlocks.get(spinlock).unwrap();

    // Keep spinning while the previous value was "already locked"
    while is_locked.swap(true, Ordering::Acquire) == true {
        hint::spin_loop();
    }
    // The lock is now ours
}

#[no_mangle]
pub unsafe fn spinlock_release_spin(spinlock_raw: *const Spinlock) {
    let spinlock: &'static Spinlock = &*spinlock_raw;
    // TODO(PT): Is SPINLOCKS.lock() itself going to act as a mutex?
    let spinlocks = SPINLOCKS.lock();
    let is_locked = spinlocks.get(spinlock).unwrap();
    is_locked.store(false, Ordering::Release);
}
