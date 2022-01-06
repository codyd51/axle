#![no_std]

extern crate alloc;
use axle_rt::{ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;
use cstr_core::CString;

pub fn copy_str_into_sized_slice(slice: &mut [u8], s: &str) -> () {
    let c_str = CString::new(s).unwrap();
    let c_str_bytes = c_str.as_bytes_with_nul();
    slice[..c_str_bytes.len()].copy_from_slice(c_str_bytes);
}

pub fn str_from_u8_nul_utf8_unchecked(utf8_src: &[u8]) -> &str {
    let nul_range_end = utf8_src
        .iter()
        .position(|&c| c == b'\0')
        .unwrap_or(utf8_src.len()); // default to length if no `\0` present
    unsafe { core::str::from_utf8_unchecked(&utf8_src[0..nul_range_end]) }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct FileManagerReadDirectory {
    pub event: u32,
    pub dir: [u8; 64],
}

impl FileManagerReadDirectory {
    pub fn new(dir: &str) -> Self {
        let mut s = FileManagerReadDirectory {
            event: Self::EXPECTED_EVENT,
            dir: [0; 64],
        };
        copy_str_into_sized_slice(&mut s.dir, dir);
        s
    }
}

impl ExpectsEventField for FileManagerReadDirectory {
    const EXPECTED_EVENT: u32 = 100;
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct FileManagerDirectoryEntry {
    pub name: [u8; 64],
    pub is_directory: bool,
}

impl FileManagerDirectoryEntry {
    pub fn new(name: &str, is_directory: bool) -> Self {
        let mut ret = FileManagerDirectoryEntry {
            name: [0; 64],
            is_directory,
        };
        copy_str_into_sized_slice(&mut ret.name, name);
        ret
    }
}

#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct FileManagerDirectoryContents {
    pub event: u32,
    pub entries: [Option<FileManagerDirectoryEntry>; 64],
}

impl ExpectsEventField for FileManagerDirectoryContents {
    const EXPECTED_EVENT: u32 = 100;
}
