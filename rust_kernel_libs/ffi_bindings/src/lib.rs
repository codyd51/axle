#![no_std]
#![feature(panic_info_message)]
#![feature(default_alloc_error_handler)]

extern crate alloc;

use alloc::alloc::{GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::panic::PanicInfo;

#[no_mangle]
pub fn add2(left: usize, right: usize) -> usize {
    left + right
}

#[panic_handler]
fn panic(panic_info: &PanicInfo<'_>) -> ! {
    let msg = match panic_info.message() {
        Some(s) => format!("{}", s),
        None => "Box<Any>".to_string(),
    };

    if let Some(location) = panic_info.location() {
        /*
        internal_println!(
            "panic occurred in file '{}' at line {}",
            location.file(),
            location.line(),
        );
        */
    }

    //let c_to_print = CString::new(msg).expect("CString::new failed");
    unsafe {
        //libc::assert(false, c_to_print.as_ptr() as *const u8);
    }
    loop {}
}

pub struct Dummy;

unsafe impl GlobalAlloc for Dummy {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        /*
        libc::printf(
            cstr_core::cstr!("Malloc %d\n").as_ptr() as *const u8,
            layout.size() as u32,
        );
        */
        //printf!("Malloc {}\n", layout.size());
        //libc::malloc(layout.size()) as *mut u8
        todo!()
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        /*
        libc::printf(
            cstr_core::cstr!("Free %d\n").as_ptr() as *const u8,
            _layout.size() as u32,
        );
        */
        //libc::free(ptr as *mut libc::c_char);
        todo!()
    }
}

#[global_allocator]
static ALLOCATOR: Dummy = Dummy;
