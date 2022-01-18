#![no_std]

extern crate alloc;
use axle_rt::{copy_str_into_sized_slice, ContainsEventField, ExpectsEventField};
use axle_rt_derive::ContainsEventField;

pub fn str_from_u8_nul_utf8_unchecked(utf8_src: &[u8]) -> &str {
    let nul_range_end = utf8_src
        .iter()
        .position(|&c| c == b'\0')
        .unwrap_or(utf8_src.len()); // default to length if no `\0` present
    unsafe { core::str::from_utf8_unchecked(&utf8_src[0..nul_range_end]) }
}

// Reading directories

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
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
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

// TODO(PT): Variable-length array at the end of this structure
#[repr(C)]
#[derive(Debug, ContainsEventField)]
pub struct FileManagerDirectoryContents {
    pub event: u32,
    pub entries: [Option<FileManagerDirectoryEntry>; 128],
}

impl ExpectsEventField for FileManagerDirectoryContents {
    const EXPECTED_EVENT: u32 = 100;
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
    const EXPECTED_EVENT: u32 = 101;
}
