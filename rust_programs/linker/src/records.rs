use alloc::{slice, vec::Vec};
use bitflags::bitflags;
use core::mem;

pub unsafe fn any_as_u8_slice<T: Sized>(p: &T) -> &[u8] {
    slice::from_raw_parts((p as *const T) as *const u8, mem::size_of::<T>())
}

pub fn copy_struct_to_slice<T: Sized>(st: &T, out: &mut [u8]) {
    let struct_bytes = unsafe { any_as_u8_slice(st) };
    out.copy_from_slice(&struct_bytes)
}

pub trait Packable {
    fn id(&self) -> usize;
    fn len(&self) -> usize;
    fn write_to_slice(&self, out: &mut [u8]);
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ElfHeader64 {
    magic: [u8; 4],
    word_size: u8,
    endianness: u8,
    elf_version: u8,
    os_abi: u8,
    os_abi_version: u8,
    reserved: [u8; 7],
    elf_type: u16,
    isa_type: u16,
    elf_version2: u32,
    pub entry_point: u64,
    pub program_header_table_start: u64,
    pub section_header_table_start: u64,
    flags: u32,
    header_size: u16,
    pub program_header_table_entry_size: u16,
    pub program_header_table_entry_count: u16,
    pub section_header_table_entry_size: u16,
    pub section_header_table_entry_count: u16,
    pub section_names_section_header_index: u16,
}

impl ElfHeader64 {
    pub fn new() -> Self {
        Self {
            magic: [0x7f, 'E' as u8, 'L' as u8, 'F' as u8],
            word_size: 2,
            endianness: 1,
            elf_version: 1,
            os_abi: 0xfe,
            os_abi_version: 1,
            reserved: [0; 7],
            elf_type: 0x02,
            isa_type: 0x3e,
            elf_version2: 1,
            // PT: The bottom fields should be filled in by the caller
            entry_point: 0,
            program_header_table_start: 0,
            section_header_table_start: 0,
            flags: 0,
            header_size: mem::size_of::<Self>() as _,
            program_header_table_entry_size: mem::size_of::<ElfSegment64>() as _,
            program_header_table_entry_count: 0,
            section_header_table_entry_size: mem::size_of::<ElfSection64>() as _,
            section_header_table_entry_count: 0,
            section_names_section_header_index: 0,
        }
    }
}

pub struct ElfHeader64Record {
    pub id: usize,
    pub inner: ElfHeader64,
}

impl ElfHeader64Record {
    pub fn new(inner: ElfHeader64) -> Self {
        Self { id: 1, inner }
    }
}

impl Packable for ElfHeader64Record {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        mem::size_of::<ElfHeader64>()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        copy_struct_to_slice(&self.inner, out)
    }
}

#[repr(C)]
#[derive(Debug)]
pub enum ElfSegmentType {
    Loadable = 1,
}

/*
#[repr(C)]
#[derive(Debug)]
enum ElfSegmentFlag {
    EXECUTABLE = 0x1,
    Writable = 0x2,
    Readable = 0x4,
}
*/
bitflags! {
    pub struct ElfSegmentFlag: u32 {
        const EXECUTABLE = 0x1;
        const WRITABLE = 0x2;
        const READABLE = 0x4;
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ElfSegment64 {
    pub segment_type: u32,
    pub flags: u32,
    pub offset: u64,
    pub vaddr: u64,
    pub paddr: u64,
    pub file_size: u64,
    pub mem_size: u64,
    pub align: u64,
}

pub struct ElfSegment64Record {
    pub id: usize,
    pub inner: ElfSegment64,
}

impl ElfSegment64Record {
    pub fn new(inner: ElfSegment64) -> Self {
        Self { id: 2, inner }
    }
}

impl Packable for ElfSegment64Record {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        mem::size_of::<ElfSegment64>()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        copy_struct_to_slice(&self.inner, out)
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ElfSection64 {
    pub name: u32,
    pub segment_type: u32,
    pub flags: u64,
    pub addr: u64,
    pub offset: u64,
    pub size: u64,
    pub link: u32,
    pub info: u32,
    pub addr_align: u64,
    pub ent_size: u64,
}

pub struct ElfSection64Record {
    pub id: usize,
    pub inner: ElfSection64,
}

impl ElfSection64Record {
    pub fn new(inner: ElfSection64) -> Self {
        Self { id: 4, inner }
    }
}

impl Packable for ElfSection64Record {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        mem::size_of::<ElfSection64>()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        copy_struct_to_slice(&self.inner, out)
    }
}

pub struct VecRecord {
    pub id: usize,
    pub inner: Vec<u8>,
}

impl VecRecord {
    pub fn new(inner: Vec<u8>) -> Self {
        Self { id: 3, inner }
    }
}

impl Packable for VecRecord {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        self.inner.len()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        out.copy_from_slice(&self.inner[..])
    }
}
