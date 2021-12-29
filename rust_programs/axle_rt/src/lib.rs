#![no_std]
#![feature(start)]
#![feature(default_alloc_error_handler)]
#![feature(slice_ptr_get)]
#![feature(panic_info_message)]

extern crate alloc;
pub extern crate libc;
use alloc::alloc::{GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::panic::PanicInfo;
pub use cstr_core;
use cstr_core::CString;

#[macro_export]
macro_rules! printf {
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args!($($arg)*));
        let c_str = ::cstr_core::CString::new(s).unwrap();
        #[allow(unused_unsafe)]
        unsafe { ::libc::printf(c_str.as_ptr() as *const u8); }
    })
}

#[allow(dead_code)]
#[derive(Debug)]
pub struct AmcMessage<'a, T> {
    source: &'a str,
    dest: &'a str,
    body: &'a T,
}

impl<T> AmcMessage<'_, T> {
    pub fn source(&self) -> &str {
        self.source
    }
    pub fn dest(&self) -> &str {
        self.source
    }
    pub fn body(&self) -> &T {
        self.body
    }
}

pub trait HasEventField {
    fn event(&self) -> u32;
}

unsafe fn amc_message_await_unchecked<T>(
    from_service: &str,
) -> Result<AmcMessage<T>, core::str::Utf8Error> {
    let from_service_c_str = CString::new(from_service).unwrap();
    let mut msg_ptr = core::ptr::null_mut();
    libc::amc_message_await(from_service_c_str.as_ptr() as *const u8, &mut msg_ptr);

    let msg_body_slice = core::ptr::slice_from_raw_parts(
        core::ptr::addr_of!((*msg_ptr).body),
        (*msg_ptr).len as usize,
    );
    let msg_body_as_ref_t: &T = &*(msg_body_slice.as_ptr() as *const T);

    Ok(AmcMessage {
        source: core::str::from_utf8(&(*msg_ptr).source)?,
        dest: core::str::from_utf8(&(*msg_ptr).dest)?,
        body: msg_body_as_ref_t,
    })
}

pub fn amc_message_await<T>(from_service: &str, expected_event: u32) -> AmcMessage<T>
where
    T: HasEventField,
{
    let msg: AmcMessage<T> = unsafe { amc_message_await_unchecked(from_service).unwrap() };
    assert_eq!(
        msg.body.event(),
        expected_event,
        "Expected event {}, but {} sent {} instead",
        expected_event,
        msg.source,
        msg.body.event()
    );
    msg
}

pub fn amc_register_service(this_service: &str) {
    unsafe {
        ::libc::amc_register_service(CString::new(this_service).unwrap().as_ptr() as *const u8);
    }
}

pub fn amc_message_send<T>(to_service: &str, message: T) {
    let to_service_c_str = CString::new(to_service).unwrap();
    let msg_ptr = &message as *const _ as *const libc::c_void;
    unsafe {
        ::libc::amc_message_send(
            to_service_c_str.as_ptr() as *const u8,
            msg_ptr,
            core::mem::size_of::<T>() as u32,
        );
    }
}

#[panic_handler]
fn panic(panic_info: &PanicInfo<'_>) -> ! {
    let msg = match panic_info.message() {
        Some(s) => format!("{}", s),
        None => "Box<Any>".to_string(),
    };

    let c_to_print = CString::new(msg).expect("CString::new failed");
    unsafe {
        libc::assert(false, c_to_print.as_ptr() as *const u8);
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
        libc::malloc(layout.size()) as *mut u8
    }
    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        /*
        libc::printf(
            cstr_core::cstr!("Free %d\n").as_ptr() as *const u8,
            _layout.size() as u32,
        );
        */
        libc::free(ptr as *mut libc::c_char);
    }
}

#[global_allocator]
static ALLOCATOR: Dummy = Dummy;
