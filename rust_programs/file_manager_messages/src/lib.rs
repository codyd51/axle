#![no_std]

extern crate alloc;

#[cfg(target_os = "axle")]
mod conditional_imports {
    pub use alloc::alloc::alloc;
    pub use alloc::alloc::Layout;
    pub use alloc::vec::Vec;
    pub use axle_rt::{amc_message_send, amc_message_send_untyped};
    pub use core::mem::align_of;
    pub use core::mem::size_of;
    pub use core::ptr::copy_nonoverlapping;
}
#[cfg(not(target_os = "axle"))]
mod conditional_imports {}

use crate::conditional_imports::*;

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

// File metadata

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct CheckFileExists {
    pub event: u32,
    pub path: [u8; 64],
}

impl CheckFileExists {
    pub fn new(path: &str) -> Self {
        let mut s = Self {
            event: Self::EXPECTED_EVENT,
            path: [0; 64],
        };
        copy_str_into_sized_slice(&mut s.path, path);
        s
    }
}

impl ExpectsEventField for CheckFileExists {
    const EXPECTED_EVENT: u32 = 103;
}

#[repr(C)]
#[derive(Debug, Copy, Clone, ContainsEventField)]
pub struct CheckFileExistsResponse {
    pub event: u32,
    pub path: [u8; 64],
    pub exists: bool,
    pub file_size: usize,
}

#[cfg(target_os = "axle")]
impl CheckFileExistsResponse {
    pub fn send(service: &str, path: &str, exists: bool, file_size: usize) {
        let mut response = CheckFileExistsResponse {
            event: CheckFileExistsResponse::EXPECTED_EVENT,
            path: [0; 64],
            exists,
            file_size,
        };
        copy_str_into_sized_slice(&mut response.path, path);
        amc_message_send(service, response);
    }
}

impl ExpectsEventField for CheckFileExistsResponse {
    const EXPECTED_EVENT: u32 = 103;
}

// Partial file reads

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct ReadFilePart {
    pub event: u32,
    pub path: [u8; 64],
    pub offset: usize,
    pub len: usize,
}

impl ReadFilePart {
    pub fn new(path: &str, offset: usize, len: usize) -> Self {
        let mut s = ReadFilePart {
            event: Self::EXPECTED_EVENT,
            path: [0; 64],
            offset,
            len,
        };
        copy_str_into_sized_slice(&mut s.path, path);
        s
    }
}

impl ExpectsEventField for ReadFilePart {
    const EXPECTED_EVENT: u32 = 104;
}

#[repr(C)]
#[derive(Debug, Copy, Clone, ContainsEventField)]
pub struct ReadFilePartResponse {
    pub event: u32,
    pub path: [u8; 64],
    pub data_len: usize,
    pub data: [u8; 0],
}

#[cfg(target_os = "axle")]
impl ReadFilePartResponse {
    pub fn send(service: &str, path: &str, data: &[u8]) {
        let total_size = size_of::<ReadFilePartResponse>() + data.len();
        let layout = Layout::from_size_align(total_size, align_of::<usize>()).unwrap();
        unsafe {
            let mut s = alloc(layout) as *mut ReadFilePartResponse;
            (*s).event = Self::EXPECTED_EVENT;
            copy_str_into_sized_slice(&mut (*s).path, path);
            (*s).data_len = data.len();
            copy_nonoverlapping(data.as_ptr(), (*s).data.as_mut_ptr(), data.len());
            amc_message_send_untyped(service, s as *const u8, total_size);
        }
    }
}

impl ExpectsEventField for ReadFilePartResponse {
    const EXPECTED_EVENT: u32 = 104;
}
