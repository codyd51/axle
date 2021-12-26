#![no_std]
#![feature(start)]
#![feature(default_alloc_error_handler)]
#![feature(panic_info_message)]

extern crate alloc;
use alloc::alloc::{GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::panic::PanicInfo;
pub use cstr_core;
use cstr_core::CString;
pub use libc;

#[panic_handler]
fn panic(panic_info: &PanicInfo<'_>) -> ! {
    let msg = match panic_info.message() {
        Some(s) => format!("{}", s),
        None => "Box<Any>".to_string(),
    };

    let c_to_print = CString::new(msg).expect("CString::new failed");
    unsafe {
        libc::assert(false, c_to_print.as_ptr());
    }
    loop {}
}

pub struct Dummy;

unsafe impl GlobalAlloc for Dummy {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        libc::printf(
            cstr_core::cstr!("Malloc %d\n").as_ptr(),
            layout.size() as u32,
        );
        libc::malloc(layout.size()) as *mut u8
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        libc::printf(
            cstr_core::cstr!("Free %d\n").as_ptr(),
            _layout.size() as u32,
        );
        libc::free(ptr as *mut libc::c_char);
    }
}

#[global_allocator]
static ALLOCATOR: Dummy = Dummy;
