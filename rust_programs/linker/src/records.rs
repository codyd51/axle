use alloc::{collections::BTreeMap, slice, vec::Vec};
use bitflags::bitflags;
use core::mem;
use cstr_core::CString;
//use std::println;

static mut _NEXT_STRUCT_ID: usize = 0;

pub fn next_struct_id() -> usize {
    unsafe {
        _NEXT_STRUCT_ID += 1;
        _NEXT_STRUCT_ID
    }
}

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
        Self { id: next_struct_id(), inner }
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
        Self { id: next_struct_id(), inner }
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

bitflags! {
    pub struct ElfSectionType: u32 {
        const PROG_BITS = 1;
        const SYMBOL_TABLE = 2;
        const STRING_TABLE = 3;
    }
    pub struct ElfSectionAttrFlag: u32 {
        const WRITE = 0x1;
        const ALLOCATE = 0x2;
        const EXEC_INSTR = 0x4;
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
        Self { id: next_struct_id(), inner }
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

#[repr(C, packed)]
#[derive(Debug, Copy, Clone)]
pub struct ElfSymbol64 {
    pub name: u32,
    pub info: u8,
    pub other: u8,
    pub owner_section_index: u16,
    pub value: u64,
    pub size: u64,
}

pub struct ElfSymbol64Record {
    pub id: usize,
    pub inner: ElfSymbol64,
}

impl ElfSymbol64Record {
    pub fn new(inner: ElfSymbol64) -> Self {
        Self { id: next_struct_id(), inner }
    }
}

impl Packable for ElfSymbol64Record {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        mem::size_of::<ElfSymbol64>()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        copy_struct_to_slice(&self.inner, out)
    }
}

pub struct SymbolTable {
    pub id: usize,
    pub inner: Vec<ElfSymbol64Record>,
}

impl SymbolTable {
    pub fn new(inner: Vec<ElfSymbol64Record>) -> Self {
        Self { id: next_struct_id(), inner }
    }
}

impl Packable for SymbolTable {
    fn id(&self) -> usize {
        self.id
    }

    fn len(&self) -> usize {
        mem::size_of::<ElfSymbol64>() * self.inner.len()
    }

    fn write_to_slice(&self, out: &mut [u8]) {
        let mut cursor = 0;
        for sym in &self.inner {
            let s = &mut out[cursor..cursor + mem::size_of::<ElfSymbol64>()];
            //println!("Writing to {cursor}, {}", sym.inner.other);
            copy_struct_to_slice(&sym.inner, s);
            cursor += mem::size_of::<ElfSymbol64>();
        }
    }
}

pub struct VecRecord {
    pub id: usize,
    pub inner: Vec<u8>,
}

impl VecRecord {
    pub fn new(inner: Vec<u8>) -> Self {
        Self { id: next_struct_id(), inner }
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

pub struct StringTableHelper {
    names: BTreeMap<usize, CString>,
    section_id_to_offsets: BTreeMap<usize, usize>,
}

impl StringTableHelper {
    pub fn new() -> Self {
        Self {
            names: BTreeMap::new(),
            section_id_to_offsets: BTreeMap::new(),
        }
    }

    pub fn add_string(&mut self, struct_id: usize, section_name: &str) {
        self.names.insert(struct_id, CString::new(section_name).unwrap());
    }

    pub fn offset_for_section_id(&self, struct_id: usize) -> usize {
        let a = *self.section_id_to_offsets.get(&struct_id).unwrap();
        //println!("Getting offset for section id {struct_id}: {a}");
        *self.section_id_to_offsets.get(&struct_id).unwrap()
    }

    pub fn render(&mut self) -> VecRecord {
        let required_space: usize = (&self.names).values().map(|s| s.as_bytes_with_nul().len()).sum();
        //println!("Required space {required_space}");

        let mut out = Vec::new();
        for _ in 0..required_space {
            out.push(0);
        }

        let mut cursor = 0;
        for (struct_id, section_name) in &self.names {
            let name = section_name.as_bytes_with_nul();
            //println!("Rendering {:?}", section_name);
            self.section_id_to_offsets.insert(*struct_id, cursor);
            out[cursor..cursor + name.len()].copy_from_slice(name);
            cursor += name.len();
        }
        //println!("Rendered {:?}", out);

        VecRecord::new(out)
    }
}
