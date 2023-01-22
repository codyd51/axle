#![no_std]
#![feature(format_args_nl)]
#![feature(panic_info_message)]
#![feature(default_alloc_error_handler)]

extern crate alloc;

use alloc::alloc::{GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::ffi::{c_char, c_void};
use core::panic::PanicInfo;
pub use cstr_core;
use cstr_core::CString;

extern "C" {
    pub fn kmalloc(size: usize) -> *mut c_void;
    pub fn kfree(ptr: *mut c_char);
    pub fn printf(fmt: *const u8, ...) -> i32;
    pub fn assert(condition: bool, message: *const u8) -> ();
    pub fn _panic(message: *const u8, file: *const u8, line: i64);
    pub fn interrupt_setup_callback(
        interrupt_vec: u8,
        callback: extern "C" fn(*const RegisterStateX86_64),
    );
    pub fn task_switch();
    pub fn task_die(exit_code: u32);
    pub fn amc_wake_sleeping_services();
}

#[macro_export]
macro_rules! printf {
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args!($($arg)*));
        for x in s.split('\0') {
            let log = ffi_bindings::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { ffi_bindings::printf(log.as_ptr() as *const u8); }
        }
    })
}

#[macro_export]
macro_rules! println {
    () => ($crate::printf!("\n"));
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args_nl!($($arg)*));
        for x in s.split('\0') {
            let log = ffi_bindings::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { ffi_bindings::printf(log.as_ptr() as *const u8); }
        }
    })
}

macro_rules! internal_println {
    () => ($crate::printf!("\n"));
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args_nl!($($arg)*));
        for x in s.split('\0') {
            let log = ::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { printf(log.as_ptr() as *const u8); }
        }
    })
}

#[panic_handler]
pub fn panic(panic_info: &PanicInfo<'_>) -> ! {
    let msg = match panic_info.message() {
        Some(s) => format!("{}", s),
        None => "Box<Any>".to_string(),
    };

    if let Some(location) = panic_info.location() {
        internal_println!(
            "panic occurred in file '{}' at line {}",
            location.file(),
            location.line(),
        );
    }

    let c_to_print = CString::new(msg).expect("CString::new failed");
    let filename = CString::new("Rust file").unwrap();
    unsafe { printf(c_to_print.as_ptr() as *const u8) };
    unsafe {
        _panic(
            c_to_print.as_ptr() as *const u8,
            filename.as_ptr() as *const u8,
            0,
        );
    }
    loop {}
}

pub struct Dummy;

unsafe impl GlobalAlloc for Dummy {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        kmalloc(layout.size()) as *mut u8
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        kfree(ptr as *mut c_char);
    }
}

#[global_allocator]
pub static ALLOCATOR: Dummy = Dummy;

#[repr(C)]
#[derive(Debug)]
pub struct RegisterStateX86_64 {
    return_ds: u64,
    rax: u64,
    rcx: u64,
    rdx: u64,
    rbx: u64,
    rbp: u64,
    rsi: u64,
    rdi: u64,
    r8: u64,
    r9: u64,
    r10: u64,
    r11: u64,
    r12: u64,
    r13: u64,
    r14: u64,
    r15: u64,
    pub int_no: u64,
    err_code: u64,
    is_external_interrupt: u64,
    pub return_rip: u64,
    cs: u64,
    rflags: u64,
    pub return_rsp: u64,
    ss: u64,
}
