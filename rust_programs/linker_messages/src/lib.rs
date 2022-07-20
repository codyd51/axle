#![no_std]

extern crate alloc;
use core::{
    alloc::Layout,
    intrinsics::copy_nonoverlapping,
    mem::{align_of, size_of},
};

use alloc::alloc::alloc;
use alloc::vec::Vec;
#[cfg(target_os = "axle")]
use axle_rt::{amc_message_send, amc_message_send_untyped};
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const LINKER_SERVICE_NAME: &'static str = "com.axle.linker";

pub fn str_from_u8_nul_utf8_unchecked(utf8_src: &[u8]) -> &str {
    let nul_range_end = utf8_src
        .iter()
        .position(|&c| c == b'\0')
        .unwrap_or(utf8_src.len()); // default to length if no `\0` present
    unsafe { core::str::from_utf8_unchecked(&utf8_src[0..nul_range_end]) }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct AssembleSource {
    pub event: u32,
    pub source: [u8; 1024],
}

#[cfg(target_os = "axle")]
impl AssembleSource {
    pub fn send(source: &str) {
        assert!(source.len() < 1024);
        let mut s = Self {
            event: Self::EXPECTED_EVENT,
            source: [0; 1024],
        };
        unsafe {
            copy_str_into_sized_slice(&mut s.source, source);
            amc_message_send(LINKER_SERVICE_NAME, s);
        }
    }
}

impl ExpectsEventField for AssembleSource {
    const EXPECTED_EVENT: u32 = 100;
}

#[repr(C)]
#[derive(Debug, Copy, Clone, ContainsEventField)]
pub struct AssembledElf {
    pub event: u32,
    pub data_len: usize,
    pub data: [u8; 0],
}

#[cfg(target_os = "axle")]
impl AssembledElf {
    pub fn send(service: &str, data: &[u8]) {
        let total_size = size_of::<Self>() + data.len();
        let layout = Layout::from_size_align(total_size, align_of::<usize>()).unwrap();
        unsafe {
            let mut s = alloc(layout) as *mut Self;
            (*s).event = Self::EXPECTED_EVENT;
            (*s).data_len = data.len();
            copy_nonoverlapping(data.as_ptr(), (*s).data.as_mut_ptr(), data.len());
            amc_message_send_untyped(service, s as *const u8, total_size);
        }
    }
}

impl ExpectsEventField for AssembledElf {
    const EXPECTED_EVENT: u32 = 101;
}
