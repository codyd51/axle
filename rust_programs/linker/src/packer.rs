use alloc::{
    slice,
    vec::{self, Vec},
};
use bitflags::bitflags;
use core::mem;
use std::println;

use crate::records::{
    any_as_u8_slice, ElfHeader64, ElfHeader64Record, ElfSegment64, ElfSegment64Record,
    ElfSegmentFlag, ElfSegmentType, Packable, VecRecord,
};

/*
uu8] {
    slice::from_raw_parts((p as *const T) as *const u8, len)
}
*/

fn copy_struct_to_vec<T: Sized>(st: &T, len: usize, vec: &mut Vec<u8>, start_idx: usize) {
    println!("Copying struct of {len} bytes to off {start_idx}");
    let struct_bytes = unsafe { any_as_u8_slice(st) };
    vec[start_idx..start_idx + len].copy_from_slice(&struct_bytes);
}

#[derive(Debug, Copy, Clone)]
struct PackRecord {
    id: usize,
    len: usize,
}

impl PackRecord {
    fn new(packable: &dyn Packable) -> Self {
        Self {
            id: packable.id(),
            len: packable.len(),
        }
    }
}

struct PackPlanner {
    contents: Vec<PackRecord>,
}

impl PackPlanner {
    fn new() -> Self {
        Self {
            contents: Vec::new(),
        }
    }

    fn append(&mut self, packable: &dyn Packable) {
        let record = PackRecord::new(packable);
        self.contents.push(record);
    }

    fn start_addr_of(&self, id: usize) -> usize {
        // Will error if an invalid ID is passed
        self.find_record(id).unwrap();

        let mut found = false;
        let mut addr = 0;
        for record in &self.contents {
            if record.id == id {
                found = true;
                break;
            }
            addr += record.len;
        }
        assert!(found, "Failed to find record with ID {}", id);
        addr
    }

    fn find_record(&self, id: usize) -> Option<&PackRecord> {
        for record in &self.contents {
            if record.id == id {
                return Some(record);
            }
        }
        assert!(false, "Failed to find record with ID {}", id);
        None
    }

    fn end_addr_of(&self, id: usize) -> usize {
        let record = self.find_record(id).unwrap();
        self.start_addr_of(id) + record.len
    }

    fn end_addr(&self) -> usize {
        let last_record = self.contents.last().unwrap();
        self.end_addr_of(last_record.id)
    }

    /*
    fn pack_elf(&self) -> Vec<u8> {
        /*
        copy_struct_to_vec(
            &file_header_record.inner,
            64,
            &mut elf,
            packer.start_addr_of(file_header_record.id),
        );
        copy_struct_to_vec(
            &loadable_segment_record.inner,
            mem::size_of::<ElfSegment64>(),
            &mut elf,
            packer.start_addr_of(loadable_segment_record.id),
        );
        */
    }
    */
}

pub fn pack_elf() -> Vec<u8> {
    // File header
    let mut file_header_record = ElfHeader64Record::new(ElfHeader64::new());

    let mut packer = PackPlanner::new();
    packer.append(&file_header_record);

    // Loadable segment
    let loadable_segment = ElfSegment64 {
        segment_type: ElfSegmentType::Loadable as _,
        flags: (ElfSegmentFlag::EXECUTABLE.bits() | ElfSegmentFlag::READABLE.bits()) as _,
        offset: 0,
        vaddr: 0x400000,
        paddr: 0x400000,
        file_size: 0,
        mem_size: 0,
        align: 0x0000000000200000,
    };
    let mut loadable_segment_record = ElfSegment64Record::new(loadable_segment);
    packer.append(&loadable_segment_record);

    // Executable code
    let code = vec![
        // mov eax, 0xd
        0xb8, 0x0d, 0x00, 0x00, 0x00, // mov ebx, 112233
        0xbb, 0x69, 0xb6, 0x01, 0x00, // int 0x80
        0xcd, 0x80,
    ];
    let code_record = VecRecord::new(code);
    packer.append(&code_record);

    let end_addr = packer.end_addr_of(code_record.id);

    let segment_addr = packer.start_addr_of(loadable_segment_record.id);

    file_header_record.inner.program_header_table_start =
        packer.start_addr_of(loadable_segment_record.id) as _;
    file_header_record.inner.program_header_table_entry_count = 1;
    file_header_record.inner.entry_point = packer.start_addr_of(code_record.id) as _;

    loadable_segment_record.inner.file_size = packer.end_addr_of(code_record.id) as _;
    loadable_segment_record.inner.mem_size = packer.end_addr_of(code_record.id) as _;

    let mut elf = Vec::new();
    let file_size = packer.end_addr();
    for _ in 0..file_size {
        elf.push(0);
    }

    let records: Vec<&dyn Packable> =
        vec![&file_header_record, &loadable_segment_record, &code_record];
    for record in &records {
        let start_addr = packer.start_addr_of(record.id());
        let elf_slice = &mut elf[start_addr..start_addr + record.len()];
        record.write_to_slice(elf_slice);
    }

    elf

    //packer.pack_elf()

    /*
    let mut elf = Vec::new();
    let file_size = packer.end_addr_of(code_record.id);
    for i in 0..file_size {
        elf.push(0);
    }

    copy_struct_to_vec(
        &file_header_record.inner,
        64,
        &mut elf,
        packer.start_addr_of(file_header_record.id),
    );
    copy_struct_to_vec(
        &loadable_segment_record.inner,
        mem::size_of::<ElfSegment64>(),
        &mut elf,
        packer.start_addr_of(loadable_segment_record.id),
    );
    /*
    copy_struct_to_vec(
        &code_record.inner,
        code_record.inner.len(),
        &mut elf,
        packer.start_addr_of(code_record.id),
    );
    */
    let code_start = packer.start_addr_of(code_record.id);
    let code_end = packer.end_addr_of(code_record.id);
    elf[code_start..code_end].copy_from_slice(&code_record.inner);

    elf
    */
}
