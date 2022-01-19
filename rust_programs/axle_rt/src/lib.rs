#![no_std]
#![feature(start)]
#![feature(default_alloc_error_handler)]
#![feature(slice_ptr_get)]
#![feature(panic_info_message)]
#![feature(stmt_expr_attributes)]

extern crate alloc;

#[cfg(target_os = "axle")]
pub extern crate libc;
use alloc::alloc::{GlobalAlloc, Layout};
use alloc::format;
use alloc::string::ToString;
use core::{cmp::min, panic::PanicInfo};
pub use cstr_core;
use cstr_core::CString;

#[macro_export]
macro_rules! printf {
    ($($arg:tt)*) => ({
        let s = alloc::fmt::format(core::format_args!($($arg)*));
        for x in s.split('\0') {
            let log = axle_rt::cstr_core::CString::new(x).expect("printf format failed");
            unsafe { axle_rt::libc::printf(log.as_ptr() as *const u8); }
        }
    })
}

pub fn copy_str_into_sized_slice(slice: &mut [u8], s: &str) -> usize {
    // Leave one byte in the slice for the NULL terminator
    let trimmed_str_len = min(s.len(), slice.len() - 1);
    let c_str = CString::new(&s[..trimmed_str_len]).unwrap();
    let c_str_bytes = c_str.as_bytes_with_nul();
    let len = c_str_bytes.len();
    slice[..len].copy_from_slice(c_str_bytes);
    len
}

#[allow(dead_code)]
#[derive(Debug)]
pub struct AmcMessage<'a, T: ?Sized> {
    source: &'a str,
    dest: &'a str,
    body: &'a T,
}

impl<T: ?Sized> AmcMessage<'_, T> {
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

pub trait ExpectsEventField {
    const EXPECTED_EVENT: u32;
}

pub trait ContainsEventField {
    fn event(&self) -> u32;
}

// This allows receiving messages for which the format of the body is not yet known
// It is the caller's responsibility to parse the body into the correct type
#[cfg(target_os = "axle")]
pub unsafe fn amc_message_await_untyped(
    from_service: Option<&str>,
) -> Result<AmcMessage<[u8]>, core::str::Utf8Error> {
    let mut msg_ptr = core::ptr::null_mut();

    // Does the caller want to await any message or just messages from a specific caller?
    if let Some(from_service) = from_service {
        let from_service_c_str = CString::new(from_service).expect("cstr new failed");
        libc::amc_message_await(from_service_c_str.as_ptr() as *const u8, &mut msg_ptr);
    } else {
        libc::amc_message_await_any(&mut msg_ptr);
    }

    // The source and destination buffers that come through will have null bytes at the end of the string
    // Trim the containers here or else we'll store excess null bytes, which causes
    // problems when creating CStrings out of them later
    let source_with_null_bytes = core::str::from_utf8(&(*msg_ptr).source)?;
    let source_without_null_bytes = source_with_null_bytes.trim_matches(char::from(0));

    let dest_with_null_bytes = core::str::from_utf8(&(*msg_ptr).dest)?;
    let dest_without_null_bytes = dest_with_null_bytes.trim_matches(char::from(0));

    let msg_body_slice = core::ptr::slice_from_raw_parts(
        core::ptr::addr_of!((*msg_ptr).body),
        (*msg_ptr).len as usize,
    );
    let msg_body_as_ref = &*(msg_body_slice as *const [u8]);

    Ok(AmcMessage {
        source: source_without_null_bytes,
        dest: dest_without_null_bytes,
        body: msg_body_as_ref,
    })
}

// This allows receiving messages that might not specify an event field
// The caller _must_ be certain that the type they've parsed
// is indeed the type that was provided in the message
#[cfg(target_os = "axle")]
pub unsafe fn amc_message_await_unchecked<T>(
    from_service: Option<&str>,
) -> Result<AmcMessage<T>, core::str::Utf8Error> {
    let msg = amc_message_await_untyped(from_service)?;
    let msg_body_as_ref_t: &T = &*(msg.body.as_ptr() as *const T);
    Ok(AmcMessage {
        source: msg.source,
        dest: msg.dest,
        body: msg_body_as_ref_t,
    })
}

#[cfg(target_os = "axle")]
pub fn amc_message_await<T>(from_service: Option<&str>) -> AmcMessage<T>
where
    T: ExpectsEventField + ContainsEventField,
{
    let msg: AmcMessage<T> = unsafe {
        amc_message_await_unchecked(from_service).expect("await_unchecked failed in await")
    };
    assert_eq!(
        msg.body.event(),
        T::EXPECTED_EVENT,
        "Expected event {}, but {} sent {} instead",
        T::EXPECTED_EVENT,
        msg.source,
        msg.body.event()
    );
    msg
}

#[cfg(target_os = "axle")]
pub fn amc_register_service(this_service: &str) {
    unsafe {
        ::libc::amc_register_service(
            CString::new(this_service)
                .expect("register_service failed")
                .as_ptr() as *const u8,
            //this_service.as_ptr() as *const u8,
        );
    }
}

#[cfg(target_os = "axle")]
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

#[cfg(target_os = "axle")]
pub unsafe fn amc_message_send_untyped(to_service: &str, message: *const u8, size: usize) {
    let to_service_c_str = CString::new(to_service).unwrap();
    ::libc::amc_message_send(
        to_service_c_str.as_ptr() as *const u8,
        message as *const libc::c_void,
        size as u32,
    );
}

#[cfg(target_os = "axle")]
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

#[cfg(target_os = "axle")]
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

#[cfg(target_os = "axle")]
#[global_allocator]
static ALLOCATOR: Dummy = Dummy;
