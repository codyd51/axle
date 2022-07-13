use alloc::vec;
use alloc::{slice, vec::Vec};
use bitflags::bitflags;
use core::{mem, ops::Index};

use crate::records::{
    any_as_u8_slice, ElfHeader64, ElfHeader64Record, ElfSection64, ElfSection64Record, ElfSectionAttrFlag, ElfSectionType, ElfSegment64, ElfSegment64Record,
    ElfSegmentFlag, ElfSegmentType, ElfSymbol64, ElfSymbol64Record, Packable, StringTableHelper, SymbolTable, VecRecord,
};

fn copy_struct_to_vec<T: Sized>(st: &T, len: usize, vec: &mut Vec<u8>, start_idx: usize) {
    //println!("Copying struct of {len} bytes to off {start_idx}");
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
        Self { contents: Vec::new() }
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

    file_header_record.inner.program_header_table_start = packer.start_addr_of(loadable_segment_record.id) as _;
    file_header_record.inner.program_header_table_entry_count = 1;
    file_header_record.inner.entry_point = loadable_segment_record.inner.vaddr + packer.start_addr_of(code_record.id) as u64;

    loadable_segment_record.inner.file_size = packer.end_addr_of(code_record.id) as _;
    loadable_segment_record.inner.mem_size = packer.end_addr_of(code_record.id) as _;

    // Place the section headers
    let mut null_section_record = ElfSection64Record::new(ElfSection64 {
        name: 0,
        segment_type: 0,
        flags: 0,
        addr: 0,
        offset: 0,
        size: 0,
        link: 0,
        info: 0,
        addr_align: 0,
        ent_size: 0,
    });

    let mut text_section_record = ElfSection64Record::new(ElfSection64 {
        // TODO(PT): How to represent something that must be filled in?
        name: 0,
        segment_type: ElfSectionType::PROG_BITS.bits(),
        flags: (ElfSectionAttrFlag::ALLOCATE.bits() | ElfSectionAttrFlag::EXEC_INSTR.bits()) as _,
        addr: file_header_record.inner.entry_point,
        offset: packer.start_addr_of(code_record.id) as u64,
        size: code_record.len() as _,
        link: 0,
        info: 0,
        addr_align: 1,
        ent_size: 0,
    });

    let mut string_table_section_record = ElfSection64Record::new(ElfSection64 {
        name: 0,
        segment_type: ElfSectionType::STRING_TABLE.bits(),
        flags: 0,
        addr: 0,
        offset: 0x000,
        size: 0x000,
        link: 0,
        info: 0,
        addr_align: 1,
        ent_size: 0,
    });

    let mut symbol_table_section_record = ElfSection64Record::new(ElfSection64 {
        name: 0,
        segment_type: ElfSectionType::SYMBOL_TABLE.bits(),
        flags: 0,
        addr: 0,
        offset: 0x000,
        size: 0x000,
        link: 3,
        info: 1,
        addr_align: 8,
        ent_size: mem::size_of::<ElfSymbol64>() as _,
    });

    // Populate symbol table
    let mut undef_symbol_record = ElfSymbol64Record::new(ElfSymbol64 {
        name: 0,
        value: 0,
        size: 0,
        info: 0,
        other: 0,
        owner_section_index: 0,
    });
    let mut start_symbol_record = ElfSymbol64Record::new(ElfSymbol64 {
        name: 0,
        value: 0,
        size: 0,
        info: 0,
        other: 0,
        owner_section_index: 0,
    });

    let mut symbol_table_helper = StringTableHelper::new();
    symbol_table_helper.add_string(undef_symbol_record.id(), "");
    symbol_table_helper.add_string(start_symbol_record.id(), "_start");
    let symbol_table_string_table = symbol_table_helper.render();
    packer.append(&symbol_table_string_table);

    // Fix up the string table section header
    string_table_section_record.inner.offset = packer.start_addr_of(symbol_table_string_table.id()) as _;
    string_table_section_record.inner.size = symbol_table_string_table.len() as _;

    // Fix up the symbols
    undef_symbol_record.inner.name = symbol_table_helper.offset_for_section_id(undef_symbol_record.id()) as _;
    start_symbol_record.inner.name = symbol_table_helper.offset_for_section_id(start_symbol_record.id()) as _;
    //println!("Set start symbol inner name to {}", start_symbol_record.inner.name);
    start_symbol_record.inner.info = 0x10;
    start_symbol_record.inner.owner_section_index = 0x1;
    start_symbol_record.inner.value = file_header_record.inner.entry_point;

    // Generate the final symbol table
    let symbol_table = SymbolTable::new(vec![undef_symbol_record, start_symbol_record]);
    packer.append(&symbol_table);

    // Fix up the symbol table section header
    symbol_table_section_record.inner.offset = packer.start_addr_of(symbol_table.id()) as _;
    symbol_table_section_record.inner.size = symbol_table.len() as _;
    //println!("Set symbol table to {}", symbol_table_section_record.inner.offset);

    // Generate the table of section header names
    let mut section_header_string_table_section_record = ElfSection64Record::new(ElfSection64 {
        name: 0,
        segment_type: ElfSectionType::STRING_TABLE.bits(),
        flags: 0,
        addr: 0,
        offset: 0,
        size: 0,
        link: 0,
        info: 0,
        addr_align: 1,
        ent_size: 0,
    });

    // Populate section names
    let mut section_header_names_helper = StringTableHelper::new();
    // The first byte is defined to be the null byte
    section_header_names_helper.add_string(null_section_record.id(), "");
    section_header_names_helper.add_string(text_section_record.id(), ".text");
    section_header_names_helper.add_string(section_header_string_table_section_record.id(), ".shstrtab");
    section_header_names_helper.add_string(symbol_table_section_record.id(), ".symtab");
    section_header_names_helper.add_string(string_table_section_record.id(), ".strtab");
    let section_header_string_table = section_header_names_helper.render();
    packer.append(&section_header_string_table);

    // Fix up the section names section header
    section_header_string_table_section_record.inner.offset = packer.start_addr_of(section_header_string_table.id()) as _;
    section_header_string_table_section_record.inner.size = section_header_string_table.len() as _;

    // Fix up the name pointers of every section header
    null_section_record.inner.name = section_header_names_helper.offset_for_section_id(null_section_record.id()) as _;
    text_section_record.inner.name = section_header_names_helper.offset_for_section_id(text_section_record.id()) as _;
    section_header_string_table_section_record.inner.name =
        section_header_names_helper.offset_for_section_id(section_header_string_table_section_record.id()) as _;
    string_table_section_record.inner.name = section_header_names_helper.offset_for_section_id(string_table_section_record.id()) as _;
    symbol_table_section_record.inner.name = section_header_names_helper.offset_for_section_id(symbol_table_section_record.id()) as _;

    let sections = vec![
        &null_section_record,
        &text_section_record,
        &symbol_table_section_record,
        &string_table_section_record,
        &section_header_string_table_section_record,
    ];
    for section in &sections {
        packer.append(*section);
    }

    // Record the section names section header in the ELF header
    file_header_record.inner.section_names_section_header_index =
        sections.iter().position(|s| s.id() == section_header_string_table_section_record.id()).unwrap() as _;

    // Record the overall section header data in the ELF header
    file_header_record.inner.section_header_table_start = packer.start_addr_of(null_section_record.id) as _;
    file_header_record.inner.section_header_table_entry_count = sections.len() as _;

    let mut elf = Vec::new();
    let file_size = packer.end_addr();
    for _ in 0..file_size {
        elf.push(0);
    }

    let records: Vec<&dyn Packable> = vec![
        &file_header_record,
        &loadable_segment_record,
        &code_record,
        &symbol_table_string_table,
        &symbol_table,
        &section_header_string_table,
        &null_section_record,
        &text_section_record,
        &symbol_table_section_record,
        &string_table_section_record,
        &section_header_string_table_section_record,
    ];
    for record in &records {
        let start_addr = packer.start_addr_of(record.id());
        //println!("writing to {start_addr}, {}", start_addr + record.len());
        let elf_slice = &mut elf[start_addr..start_addr + record.len()];
        record.write_to_slice(elf_slice);
    }

    elf
}
