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
use axle_rt::amc_message_send_untyped;
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub const FILE_SERVER_SERVICE_NAME: &'static str = "com.axle.file_server";

pub fn str_from_u8_nul_utf8_unchecked(utf8_src: &[u8]) -> &str {
    let nul_range_end = utf8_src
        .iter()
        .position(|&c| c == b'\0')
        .unwrap_or(utf8_src.len()); // default to length if no `\0` present
    unsafe { core::str::from_utf8_unchecked(&utf8_src[0..nul_range_end]) }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct ReadFile {
    pub event: u32,
    pub path: [u8; 64],
}

impl ReadFile {
    pub fn new(path: &str) -> Self {
        let mut s = ReadFile {
            event: Self::EXPECTED_EVENT,
            path: [0; 64],
        };
        copy_str_into_sized_slice(&mut s.path, path);
        s
    }
}

impl ExpectsEventField for ReadFile {
    const EXPECTED_EVENT: u32 = 100;
}

#[repr(C)]
#[derive(Debug, Copy, Clone, ContainsEventField)]
pub struct ReadFileResponse {
    pub event: u32,
    pub path: [u8; 64],
    pub len: usize,
    pub data: [u8; 0],
}

#[cfg(target_os = "axle")]
impl ReadFileResponse {
    pub fn send(service: &str, path: &str, data: &Vec<u8>) {
        let total_size = size_of::<ReadFileResponse>() + data.len();
        let layout = Layout::from_size_align(total_size, align_of::<usize>()).unwrap();
        unsafe {
            let mut s = alloc(layout) as *mut ReadFileResponse;
            (*s).event = Self::EXPECTED_EVENT;
            copy_str_into_sized_slice(&mut (*s).path, path);
            (*s).len = data.len();
            copy_nonoverlapping(data.as_ptr(), (*s).data.as_mut_ptr(), data.len());
            amc_message_send_untyped(service, s as *const u8, total_size);
        }
    }
}

impl ExpectsEventField for ReadFileResponse {
    const EXPECTED_EVENT: u32 = 100;
}

// Reading directories

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct ReadDirectory {
    pub event: u32,
    pub dir: [u8; 64],
}

impl ReadDirectory {
    pub fn new(dir: &str) -> Self {
        let mut s = ReadDirectory {
            event: Self::EXPECTED_EVENT,
            dir: [0; 64],
        };
        copy_str_into_sized_slice(&mut s.dir, dir);
        s
    }
}

impl ExpectsEventField for ReadDirectory {
    const EXPECTED_EVENT: u32 = 101;
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct DirectoryEntry {
    pub name: [u8; 64],
    pub is_directory: bool,
}

impl DirectoryEntry {
    pub fn new(name: &str, is_directory: bool) -> Self {
        let mut ret = DirectoryEntry {
            name: [0; 64],
            is_directory,
        };
        copy_str_into_sized_slice(&mut ret.name, name);
        ret
    }
}

// TODO(PT): Variable-length array at the end of this structure
// Ref: https://www.reddit.com/r/rust/comments/75pnn2/ffi_and_variable_size_data_structure/
#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct DirectoryContents {
    pub event: u32,
    pub entries: [Option<DirectoryEntry>; 128],
}

impl ExpectsEventField for DirectoryContents {
    const EXPECTED_EVENT: u32 = 101;
}

// Launching files

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct LaunchProgram {
    pub event: u32,
    pub path: [u8; 64],
}

impl LaunchProgram {
    pub fn new(path: &str) -> Self {
        let mut s = Self {
            event: Self::EXPECTED_EVENT,
            path: [0; 64],
        };
        copy_str_into_sized_slice(&mut s.path, path);
        s
    }
}

impl ExpectsEventField for LaunchProgram {
    const EXPECTED_EVENT: u32 = 102;
}
