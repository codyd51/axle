use crate::{print, println};
use alloc::{
    borrow::ToOwned,
    boxed::Box,
    collections::BTreeMap,
    rc::Rc,
    string::{String, ToString},
    vec,
};
use alloc::{slice, vec::Vec};
use bitflags::bitflags;
use core::{cell::RefCell, fmt::Display, mem, ops::Index};
use cstr_core::CString;

use crate::{
    assembly_packer::{DataPacker, InstructionPacker, SymbolOffset, SymbolSize},
    records::{
        any_as_u8_slice, ElfHeader64, ElfHeader64Record, ElfSection64, ElfSection64Record, ElfSectionAttrFlag, ElfSectionType, ElfSectionType2, ElfSegment64,
        ElfSegment64Record, ElfSegmentFlag, ElfSegmentType, ElfSymbol64, ElfSymbol64Record, Packable, StringTableHelper, SymbolTable, VecRecord,
    },
    symbols::{DataSymbol, SymbolId, SymbolType},
};

struct ReadOnlyData {
    data_packer: Rc<RefCell<DataPacker>>,
    rendered_data: RefCell<Vec<u8>>,
}

impl ReadOnlyData {
    fn new(data_packer: &Rc<RefCell<DataPacker>>) -> Self {
        Self {
            data_packer: Rc::clone(data_packer),
            rendered_data: RefCell::new(Vec::new()),
        }
    }
}

impl MagicPackable for ReadOnlyData {
    fn len(&self) -> usize {
        self.rendered_data.borrow().len()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::ReadOnlyDataSection
    }

    fn prerender(&self, layout: &FileLayout) {
        let mut rendered_data = self.rendered_data.borrow_mut();
        let data_packer = self.data_packer.borrow();
        for (_, _, symbol) in data_packer.symbols.iter() {
            let mut symbol_bytes = symbol.render(layout, rendered_data.len());
            // Some symbols may be rendered to immediate values in the symbol table
            if let Some(mut symbol_bytes) = symbol_bytes {
                rendered_data.append(&mut symbol_bytes);
            }
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        self.rendered_data.borrow().to_owned()
    }
}

//impl<T: MagicPackable + ?Sized> MagicSectionHeader for T {}

pub struct FileLayout {
    virtual_base: u64,
    contents: RefCell<Vec<Rc<dyn MagicPackable>>>,
    segment_headers: RefCell<Vec<Rc<dyn MagicPackable>>>,
    section_headers: RefCell<Vec<Rc<dyn MagicSectionHeader>>>,
    section_header_names_string_table: RefCell<Option<Rc<MagicSectionHeaderNamesStringsTable>>>,
    symbol_table: RefCell<Option<Rc<MagicSymbolTable>>>,
    string_table: RefCell<Option<Rc<MagicStringTable>>>,
}

impl FileLayout {
    pub fn new(virtual_base: u64) -> Self {
        Self {
            virtual_base,
            contents: RefCell::new(Vec::new()),
            segment_headers: RefCell::new(Vec::new()),
            section_headers: RefCell::new(Vec::new()),
            section_header_names_string_table: RefCell::new(None),
            symbol_table: RefCell::new(None),
            string_table: RefCell::new(None),
        }
    }

    fn set_section_header_names_string_table(&self, section_header_names_string_table: &Rc<MagicSectionHeaderNamesStringsTable>) {
        let mut strtab = self.section_header_names_string_table.borrow_mut();
        assert!(strtab.is_none(), "Cannot set section header names string table twice");
        *strtab = Some(Rc::clone(section_header_names_string_table));
        self.append(&(Rc::clone(section_header_names_string_table) as Rc<dyn MagicPackable>));
    }

    fn set_string_table(&self, string_table: &Rc<MagicStringTable>) {
        let mut strtab = self.string_table.borrow_mut();
        assert!(strtab.is_none(), "Cannot set string table twice");
        *strtab = Some(Rc::clone(string_table));
        self.append(&(Rc::clone(string_table) as Rc<dyn MagicPackable>));
    }

    fn set_symbol_table(&self, symbol_table: &Rc<MagicSymbolTable>) {
        let mut symtab = self.symbol_table.borrow_mut();
        assert!(symtab.is_none(), "Cannot set symbol table twice");
        *symtab = Some(Rc::clone(symbol_table));
        self.append(&(Rc::clone(symbol_table) as Rc<dyn MagicPackable>));
    }

    fn render(&self) -> Vec<u8> {
        println!("Rendering ELF");
        let contents = self.contents.borrow();

        for packable in contents.iter() {
            packable.prerender(&self);
        }

        let mut out = vec![0; 0];
        let mut cursor = 0;
        for packable in contents.iter() {
            // Render the packable to bytes
            let mut rendered_packable = packable.render(self);

            let start = out.len();
            out.append(&mut rendered_packable);
            let end = out.len();

            println!("Rendered {start:016x}..{end:016x}");
        }
        out
    }

    fn append(&self, packable: &Rc<dyn MagicPackable>) {
        let mut contents = self.contents.borrow_mut();
        contents.push(Rc::clone(packable));
    }

    fn append_section_header(&self, section_header: Rc<dyn MagicSectionHeader>) {
        let mut section_headers = self.section_headers.borrow_mut();
        section_headers.push(Rc::clone(&section_header));
        self.append(&(section_header as Rc<dyn MagicPackable>));
    }

    fn append_segment_header(&self, segment_header: Rc<dyn MagicPackable>) {
        let mut segment_headers = self.segment_headers.borrow_mut();
        segment_headers.push(Rc::clone(&segment_header));
        self.append(&segment_header);
    }

    // It might be important that get_rebased_value(), start_of(), len()
    // aren't called until everything is set up.
    // The string section won't know how big it is until we've registered all the section headers and data symbols
    fn static_start_of(&self, target: RebaseTarget) -> usize {
        let mut cursor = 0;
        let contents = self.contents.borrow();
        for packable in contents.iter() {
            if packable.struct_type() == target {
                return cursor;
            }
            cursor += packable.len();
        }
        panic!("Failed to find a struct with the provided type {target:?}");
    }

    fn static_start_of_packable(&self, target_packable: Rc<dyn MagicPackable>) -> usize {
        let mut cursor = 0;
        let contents = self.contents.borrow();
        for packable in contents.iter() {
            if Rc::ptr_eq(packable, &target_packable) {
                return cursor;
            }
            cursor += packable.len();
        }
        panic!("Failed to find the provided packable");
    }

    fn static_end_of(&self, target: RebaseTarget) -> usize {
        let target_packable = self.get_target(target);
        self.static_start_of(target) + target_packable.len()
    }

    fn virt_start_of(&self, target: RebaseTarget) -> usize {
        return self.static_start_of(target) + (self.virtual_base as usize);
    }

    fn get_target(&self, target: RebaseTarget) -> Rc<dyn MagicPackable> {
        let contents = self.contents.borrow();
        let target_packables: Vec<&Rc<dyn MagicPackable>> = contents.iter().filter(|p| p.struct_type() == target).collect();
        assert!(target_packables.len() == 1, "Expected exactly one struct with the provided type");
        let target_packable = target_packables[0];
        Rc::clone(target_packable)
    }

    fn size_of(&self, target: RebaseTarget) -> usize {
        let contents = self.contents.borrow();
        let target_packable = self.get_target(target);
        target_packable.len()
    }

    fn get_rebased_section_header_name_offset(&self, section_header: &dyn MagicSectionHeader) -> usize {
        let maybe_strtab = self.section_header_names_string_table.borrow();
        match &*maybe_strtab {
            Some(strtab) => strtab.offset_of_name(section_header.name()),
            None => panic!("Expected a section header names strings table to be set up"),
        }
    }

    fn get_rebased_symbol_name_offset(&self, symbol: &MagicElfSymbol64) -> usize {
        let maybe_strtab = self.string_table.borrow();
        match &*maybe_strtab {
            Some(strtab) => strtab.offset_of_name(&symbol.name),
            None => panic!("Expected a string table to be set up"),
        }
    }

    fn get_rebased_symbol_owner_section_index(&self, symbol: &MagicElfSymbol64) -> usize {
        match symbol.symbol_type {
            SymbolType::NullSymbol => 0,
            SymbolType::DataSymbol => self.section_header_index(SectionHeaderType::ReadOnlyDataSection),
            SymbolType::CodeSymbol => self.section_header_index(SectionHeaderType::TextSection),
        }
    }

    fn section_header_index(&self, section_header_type: SectionHeaderType) -> usize {
        let section_headers = self.section_headers.borrow();
        let section_header_indexes_matching_type: Vec<usize> = section_headers
            .iter()
            .enumerate()
            // Filter to the desired section type
            .filter(|(i, sh)| sh.section_header_type() == section_header_type)
            // Only retain the index
            .map(|(i, sh)| i)
            .collect();
        assert!(
            section_header_indexes_matching_type.len() == 1,
            "Expected exactly one section header with type {:?}, found {}",
            section_header_type,
            section_header_indexes_matching_type.len()
        );
        section_header_indexes_matching_type[0]
    }

    fn virt_offset_within(&self, target: RebaseTarget, offset: usize) -> usize {
        //println!("offset within {target:?} of {offset}");
        self.virt_start_of(target) + offset
    }

    pub fn get_rebased_value(&self, rebased_value: RebasedValue) -> usize {
        match rebased_value {
            RebasedValue::VirtStartOf(target) => self.virt_start_of(target),
            RebasedValue::StaticStartOf(target) => self.static_start_of(target),
            RebasedValue::StaticEndOf(target) => self.static_end_of(target),
            RebasedValue::VirtOffsetWithin(target, offset) => self.virt_offset_within(target, offset),
            RebasedValue::SectionHeaderIndex(section_header_type) => self.section_header_index(section_header_type),
            RebasedValue::SectionHeaderCount => self.section_headers.borrow().len(),
            RebasedValue::SizeOf(target) => self.size_of(target),
            RebasedValue::SectionHeaderName => panic!("Use get_rebased_section_header_name_offset"),
            RebasedValue::Literal(value) => value,
            RebasedValue::VirtualBase => self.virtual_base.try_into().unwrap(),
            RebasedValue::SectionHeadersHead => {
                let section_headers = self.section_headers.borrow();
                let first_section_header = Rc::clone(section_headers.first().unwrap());
                self.static_start_of_packable(first_section_header as Rc<dyn MagicPackable>)
            }
            RebasedValue::SegmentHeadersHead => {
                let segment_headers = self.segment_headers.borrow();
                let first_segment_header = Rc::clone(segment_headers.first().unwrap());
                self.static_start_of_packable(first_segment_header)
            }
            RebasedValue::SegmentHeaderCount => self.segment_headers.borrow().len(),
            RebasedValue::ReadOnlyDataSymbolVirtAddr(symbol_id) => self.read_only_data_symbol_virt_addr(symbol_id),
            RebasedValue::ReadOnlyDataSymbolVirtEnd(symbol_id) => self.read_only_data_symbol_virt_end(symbol_id),
            RebasedValue::ReadOnlyDataSymbolValue(symbol_id) => self.read_only_data_symbol_value(symbol_id),
        }
    }

    pub fn get_symbol_id_for_symbol_name(&self, symbol_name: &str) -> SymbolId {
        // PT: This abstraction only exists because keeping the symbol name as a String in RebasedValue
        // would mean that RebasedValue can no longer be Copy.
        let maybe_symbol_table = self.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        symbol_table.id_of_symbol(symbol_name)
    }

    pub fn get_symbol_entry_type_for_symbol_name(&self, symbol_name: &str) -> SymbolEntryType {
        let maybe_symbol_table = self.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        symbol_table.type_of_symbol(symbol_name)
    }

    fn read_only_data_symbol_virt_addr(&self, symbol_id: SymbolId) -> usize {
        let maybe_symbol_table = self.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        let (offset, size, symbol) = symbol_table.symbol_with_id(symbol_id);
        //println!("size {size}");
        assert!(size > 0, "Zero-sized symbols should not use this API");
        self.virt_offset_within(RebaseTarget::ReadOnlyDataSection, offset)
    }

    fn read_only_data_symbol_virt_end(&self, symbol_id: SymbolId) -> usize {
        let maybe_symbol_table = self.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        let (offset, size, symbol) = symbol_table.symbol_with_id(symbol_id);
        assert!(size > 0, "Zero-sized symbols should not use this API");
        self.virt_offset_within(RebaseTarget::ReadOnlyDataSection, offset) + size
    }

    fn read_only_data_symbol_value(&self, symbol_id: SymbolId) -> usize {
        let maybe_symbol_table = self.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        let (offset, size, symbol) = symbol_table.symbol_with_id(symbol_id);
        let immediate_value = symbol.immediate_value.borrow();
        immediate_value.unwrap() as _
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum RebaseTarget {
    ElfHeader,
    ElfSegmentHeader,
    ElfSectionHeader,
    TextSection,
    ReadOnlyDataSection,
    SymbolTableSection,
    StringsTable,
    SectionHeadersStringsTable,
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum SectionHeaderType {
    NullSection,
    ReadOnlyDataSection,
    TextSection,
    SymbolTableSection,
    StringTableSection,
    SectionHeaderNamesSectionHeader,
}

#[derive(Debug, Copy, Clone)]
pub enum RebasedValue {
    VirtStartOf(RebaseTarget),
    StaticStartOf(RebaseTarget),
    StaticEndOf(RebaseTarget),
    VirtOffsetWithin(RebaseTarget, usize),
    SectionHeaderIndex(SectionHeaderType),
    SizeOf(RebaseTarget),
    Literal(usize),
    VirtualBase,
    SegmentHeadersHead,
    SegmentHeaderCount,
    SectionHeadersHead,
    SectionHeaderCount,
    ReadOnlyDataSymbolVirtAddr(SymbolId),
    ReadOnlyDataSymbolVirtEnd(SymbolId),
    ReadOnlyDataSymbolValue(SymbolId),
    // Only for MagicSectionHeader
    SectionHeaderName,
}

pub trait MagicPackable {
    fn len(&self) -> usize;
    // PT: To remove?
    fn struct_type(&self) -> RebaseTarget;
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
    fn prerender(&self, layout: &FileLayout) {}
}

trait MagicSectionHeader: MagicPackable {
    fn name(&self) -> &str;
    fn section_header_type(&self) -> SectionHeaderType;
}

pub struct MagicElfHeader64 {
    pub entry_point: RebasedValue,
    pub program_header_table_start: RebasedValue,
    pub section_header_table_start: RebasedValue,
    pub section_header_count: RebasedValue,
}

impl MagicElfHeader64 {
    pub fn new() -> Self {
        Self {
            entry_point: RebasedValue::VirtStartOf(RebaseTarget::TextSection),
            program_header_table_start: RebasedValue::SegmentHeadersHead,
            section_header_table_start: RebasedValue::SectionHeadersHead,
            section_header_count: RebasedValue::SectionHeaderCount,
        }
    }
}

impl MagicPackable for MagicElfHeader64 {
    fn len(&self) -> usize {
        mem::size_of::<ElfHeader64>()
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = ElfHeader64::new();
        out.entry_point = layout.get_rebased_value(self.entry_point) as u64;
        out.section_header_table_entry_count = layout.get_rebased_value(self.section_header_count) as u16;
        out.section_header_table_start = layout.get_rebased_value(self.section_header_table_start) as u64;
        out.program_header_table_start = layout.get_rebased_value(RebasedValue::SegmentHeadersHead) as u64;
        out.program_header_table_entry_count = layout.get_rebased_value(RebasedValue::SegmentHeaderCount) as _;
        out.section_names_section_header_index =
            layout.get_rebased_value(RebasedValue::SectionHeaderIndex(SectionHeaderType::SectionHeaderNamesSectionHeader)) as _;

        let struct_bytes = unsafe { any_as_u8_slice(&out) };
        struct_bytes.to_owned()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::ElfHeader
    }
}

pub struct MagicSegmentHeader64 {
    segment_type: ElfSegmentType,
    flags: Vec<ElfSegmentFlag>,
    offset: RebasedValue,
    vaddr: RebasedValue,
    paddr: RebasedValue,
    file_size: RebasedValue,
    mem_size: RebasedValue,
    align: RebasedValue,
}

impl MagicSegmentHeader64 {
    pub fn text_segment_header() -> Self {
        Self::new(
            ElfSegmentType::Loadable,
            vec![ElfSegmentFlag::EXECUTABLE | ElfSegmentFlag::READABLE],
            RebasedValue::Literal(0),
            RebasedValue::VirtualBase,
            RebasedValue::VirtualBase,
            RebasedValue::StaticEndOf(RebaseTarget::TextSection),
            RebasedValue::StaticEndOf(RebaseTarget::TextSection),
            RebasedValue::Literal(0x200000),
        )
    }

    pub fn new(
        segment_type: ElfSegmentType,
        flags: Vec<ElfSegmentFlag>,
        offset: RebasedValue,
        vaddr: RebasedValue,
        paddr: RebasedValue,
        file_size: RebasedValue,
        mem_size: RebasedValue,
        align: RebasedValue,
    ) -> Self {
        Self {
            segment_type,
            flags,
            offset,
            vaddr,
            paddr,
            file_size,
            mem_size,
            align,
        }
    }
}

impl MagicPackable for MagicSegmentHeader64 {
    fn len(&self) -> usize {
        mem::size_of::<ElfSegment64>()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::ElfSegmentHeader
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = ElfSegment64 {
            segment_type: self.segment_type as _,
            flags: self.flags.iter().fold(0, |acc, f| acc | (f.bits())) as _,
            offset: layout.get_rebased_value(self.offset) as _,
            vaddr: layout.get_rebased_value(self.vaddr) as _,
            paddr: layout.get_rebased_value(self.paddr) as _,
            file_size: layout.get_rebased_value(self.file_size) as _,
            mem_size: layout.get_rebased_value(self.mem_size) as _,
            align: layout.get_rebased_value(self.align) as _,
        };

        let struct_bytes = unsafe { any_as_u8_slice(&out) };
        struct_bytes.to_owned()
    }
}

pub struct MagicElfSection64 {
    section_header_type: SectionHeaderType,
    name: String,
    segment_type: ElfSectionType2,
    flags: Vec<ElfSectionAttrFlag>,
    addr: RebasedValue,
    offset: RebasedValue,
    size: RebasedValue,
    link: RebasedValue,
    info: RebasedValue,
    addr_align: u64,
    ent_size: u64,
}

impl MagicElfSection64 {
    fn null_section_header() -> Self {
        Self::new(
            SectionHeaderType::NullSection,
            "",
            ElfSectionType2::Null,
            vec![],
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            0,
            0,
        )
    }

    fn text_section_header() -> Self {
        Self::new(
            SectionHeaderType::TextSection,
            ".text",
            ElfSectionType2::ProgBits,
            vec![ElfSectionAttrFlag::ALLOCATE, ElfSectionAttrFlag::EXEC_INSTR],
            RebasedValue::VirtStartOf(RebaseTarget::TextSection),
            RebasedValue::StaticStartOf(RebaseTarget::TextSection),
            RebasedValue::SizeOf(RebaseTarget::TextSection),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            1,
            0,
        )
    }

    fn section_header_names_section_header() -> Self {
        Self::new(
            SectionHeaderType::SectionHeaderNamesSectionHeader,
            ".shstrtab",
            ElfSectionType2::StringTable,
            vec![],
            RebasedValue::Literal(0),
            RebasedValue::StaticStartOf(RebaseTarget::SectionHeadersStringsTable),
            RebasedValue::SizeOf(RebaseTarget::SectionHeadersStringsTable),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            1,
            0,
        )
    }

    fn symbol_table_section_header() -> Self {
        Self::new(
            SectionHeaderType::SymbolTableSection,
            ".symtab",
            ElfSectionType2::SymbolTable,
            vec![],
            RebasedValue::Literal(0),
            RebasedValue::StaticStartOf(RebaseTarget::SymbolTableSection),
            RebasedValue::SizeOf(RebaseTarget::SymbolTableSection),
            // The names of these symbols will be found in the strings table
            // See ELF spec §Figure 1-13, sh_link and sh_info interpretation
            RebasedValue::SectionHeaderIndex(SectionHeaderType::StringTableSection),
            // "One greater than the symbol table index of the last local symbol (binding STB_LOCAL)"
            RebasedValue::Literal(1),
            8,
            mem::size_of::<ElfSymbol64>() as _,
        )
    }

    fn string_table_section_header() -> Self {
        Self::new(
            SectionHeaderType::StringTableSection,
            ".strtab",
            ElfSectionType2::StringTable,
            vec![],
            RebasedValue::Literal(0),
            RebasedValue::StaticStartOf(RebaseTarget::StringsTable),
            RebasedValue::SizeOf(RebaseTarget::StringsTable),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            1,
            0,
        )
    }

    fn read_only_data_section_header() -> Self {
        Self::new(
            SectionHeaderType::ReadOnlyDataSection,
            ".rodata",
            ElfSectionType2::ProgBits,
            vec![ElfSectionAttrFlag::ALLOCATE],
            RebasedValue::VirtStartOf(RebaseTarget::ReadOnlyDataSection),
            RebasedValue::StaticStartOf(RebaseTarget::ReadOnlyDataSection),
            RebasedValue::SizeOf(RebaseTarget::ReadOnlyDataSection),
            RebasedValue::Literal(0),
            RebasedValue::Literal(0),
            1,
            0,
        )
    }

    fn new(
        section_header_type: SectionHeaderType,
        name: &str,
        section_type: ElfSectionType2,
        flags: Vec<ElfSectionAttrFlag>,
        addr: RebasedValue,
        offset: RebasedValue,
        size: RebasedValue,
        link: RebasedValue,
        info: RebasedValue,
        addr_align: u64,
        ent_size: u64,
    ) -> Self {
        Self {
            section_header_type,
            name: name.to_string(),
            segment_type: section_type,
            flags,
            addr,
            offset,
            size,
            link,
            info,
            addr_align,
            ent_size,
        }
    }
}

impl MagicPackable for MagicElfSection64 {
    fn len(&self) -> usize {
        mem::size_of::<ElfSection64>()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::ElfSectionHeader
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let out = ElfSection64 {
            name: layout.get_rebased_section_header_name_offset(self) as _,
            segment_type: self.segment_type as _,
            flags: self.flags.iter().fold(0, |acc, f| acc | (f.bits())) as _,
            addr: layout.get_rebased_value(self.addr) as _,
            offset: layout.get_rebased_value(self.offset) as _,
            size: layout.get_rebased_value(self.size) as _,
            link: layout.get_rebased_value(self.link) as _,
            info: layout.get_rebased_value(self.info) as _,
            addr_align: self.addr_align,
            ent_size: self.ent_size,
        };

        let struct_bytes = unsafe { any_as_u8_slice(&out) };
        struct_bytes.to_owned()
    }
}

impl MagicSectionHeader for MagicElfSection64 {
    fn name(&self) -> &str {
        &self.name
    }

    fn section_header_type(&self) -> SectionHeaderType {
        self.section_header_type
    }
}

pub struct MagicSectionHeaderNamesStringsTable {
    names_lookup: RefCell<BTreeMap<String, usize>>,
    rendered_strings: RefCell<Vec<u8>>,
}

impl MagicSectionHeaderNamesStringsTable {
    pub fn new() -> Self {
        Self {
            names_lookup: RefCell::new(BTreeMap::new()),
            rendered_strings: RefCell::new(Vec::new()),
        }
    }

    pub fn offset_of_name(&self, name: &str) -> usize {
        *self.names_lookup.borrow().get(name).unwrap()
    }
}

impl MagicPackable for MagicSectionHeaderNamesStringsTable {
    fn len(&self) -> usize {
        //println!("Len {}", self.rendered_strings.borrow().len());
        self.rendered_strings.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
        // Perhaps we need a .strings()
        // For section headers it'd just be 1 string at index 0
        // For symbols it'd be more
        //println!("Inside prerender");
        let mut rendered_strings = self.rendered_strings.borrow_mut();
        let section_headers = layout.section_headers.borrow();
        for section_header in section_headers.iter() {
            let section_name = section_header.name();
            let section_name_c_str = CString::new(section_name).unwrap();
            let mut section_name_bytes = section_name_c_str.into_bytes_with_nul();
            let section_name_start = rendered_strings.len();
            println!("\t{section_name} in shstrtab @ {section_name_start:x}");
            self.names_lookup.borrow_mut().insert(section_name.to_string(), section_name_start);
            rendered_strings.append(&mut section_name_bytes);
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        self.rendered_strings.borrow().clone()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::SectionHeadersStringsTable
    }
}

#[derive(Debug)]
struct MagicElfSymbol64 {
    pub symbol_type: SymbolType,
    pub name: String,
    symbol_entry_type: SymbolEntryType,
    symbol_id: SymbolId,
}

impl MagicElfSymbol64 {
    fn new(symbol_type: SymbolType, name: &str, symbol_entry_type: SymbolEntryType, symbol_id: SymbolId) -> Self {
        Self {
            symbol_type,
            name: name.to_string(),
            symbol_entry_type,
            symbol_id,
        }
    }
}

#[derive(Debug)]
pub enum SymbolEntryType {
    SymbolWithBackingData,
    SymbolWithInlinedValue,
}

struct MagicSymbolTable {
    data_packer: Rc<RefCell<DataPacker>>,
    instruction_packer: Rc<InstructionPacker>,
    symbols: RefCell<Vec<MagicElfSymbol64>>,
}

impl MagicSymbolTable {
    fn new(data_packer: &Rc<RefCell<DataPacker>>, instruction_packer: &Rc<InstructionPacker>) -> Self {
        Self {
            data_packer: Rc::clone(data_packer),
            instruction_packer: Rc::clone(instruction_packer),
            symbols: RefCell::new(Vec::new()),
        }
    }

    fn id_of_symbol(&self, symbol_name: &str) -> SymbolId {
        let data_packer = self.data_packer.borrow();
        for (i, (_, _, symbol)) in data_packer.symbols.iter().enumerate() {
            if symbol.name == symbol_name {
                return SymbolId(SymbolType::DataSymbol, i);
            }
        }
        panic!("Failed to find symbol with name {symbol_name}");
    }

    fn symbol_with_id(&self, symbol_id: SymbolId) -> (SymbolOffset, SymbolSize, Rc<DataSymbol>) {
        // TODO(PT): Perhaps, in the future, the SymbolId can carry the source section of the symbol
        let data_packer = self.data_packer.borrow();
        match symbol_id.0 {
            SymbolType::NullSymbol => panic!("null symbol {symbol_id:?}"),
            SymbolType::DataSymbol => {
                let (offset, size, sym) = &data_packer.symbols[symbol_id.1];
                (*offset, *size, Rc::clone(sym))
            }
            SymbolType::CodeSymbol => todo!(),
        }
    }

    fn type_of_symbol(&self, symbol_name: &str) -> SymbolEntryType {
        let (_, size, symbol) = self.symbol_with_id(self.id_of_symbol(symbol_name));
        match size {
            0 => SymbolEntryType::SymbolWithInlinedValue,
            _ => SymbolEntryType::SymbolWithBackingData,
        }
    }
}

impl MagicPackable for MagicSymbolTable {
    fn len(&self) -> usize {
        self.symbols.borrow().len() * mem::size_of::<ElfSymbol64>()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::SymbolTableSection
    }

    fn prerender(&self, layout: &FileLayout) {
        //println!("Pre-rendering symbol table!");
        let data_packer = self.data_packer.borrow();
        let mut symbols = self.symbols.borrow_mut();

        // Write the null symbol
        symbols.push(MagicElfSymbol64::new(
            SymbolType::NullSymbol,
            "",
            SymbolEntryType::SymbolWithBackingData,
            SymbolId::null_id(),
        ));

        for (_, _, data_symbol) in data_packer.symbols.iter() {
            let symbol_entry_type = layout.get_symbol_entry_type_for_symbol_name(&data_symbol.name);
            let symbol_id = layout.get_symbol_id_for_symbol_name(&data_symbol.name);
            symbols.push(MagicElfSymbol64::new(SymbolType::DataSymbol, &data_symbol.name, symbol_entry_type, symbol_id));
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        //println!("Rendering symbol table");
        let mut out = vec![];
        let symbols = self.symbols.borrow();
        for (i, symbol) in symbols.iter().enumerate() {
            let rendered_symbol = ElfSymbol64 {
                name: layout.get_rebased_symbol_name_offset(symbol) as _,
                info: 0 as _,
                other: 0 as _,
                owner_section_index: match symbol.symbol_entry_type {
                    SymbolEntryType::SymbolWithBackingData => layout.get_rebased_symbol_owner_section_index(symbol) as _,
                    // Defined by the ELF spec
                    SymbolEntryType::SymbolWithInlinedValue => 0xfff1,
                },
                value: match symbol.symbol_type {
                    SymbolType::DataSymbol => layout.read_only_data_symbol_value(symbol.symbol_id) as _,
                    SymbolType::NullSymbol => 0 as _,
                    _ => todo!(),
                },
                size: 0 as _,
            };
            let mut rendered_symbol_bytes = unsafe { any_as_u8_slice(&rendered_symbol) }.to_owned();
            out.append(&mut rendered_symbol_bytes);
        }
        out
    }
}

pub struct MagicStringTable {
    names_lookup: RefCell<BTreeMap<String, usize>>,
    rendered_strings: RefCell<Vec<u8>>,
}

impl MagicStringTable {
    pub fn new() -> Self {
        Self {
            names_lookup: RefCell::new(BTreeMap::new()),
            rendered_strings: RefCell::new(Vec::new()),
        }
    }

    pub fn offset_of_name(&self, name: &str) -> usize {
        *self.names_lookup.borrow().get(name).unwrap()
    }
}

impl MagicPackable for MagicStringTable {
    fn len(&self) -> usize {
        //println!("Len of string table {}", self.rendered_strings.borrow().len());
        self.rendered_strings.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
        //println!("Inside prerender for string table");
        let mut rendered_strings = self.rendered_strings.borrow_mut();
        let maybe_symbol_table = layout.symbol_table.borrow();
        let symbol_table = maybe_symbol_table.as_ref().unwrap();
        let symbols = symbol_table.symbols.borrow();
        for symbol in symbols.iter() {
            let symbol_name = symbol.name.to_string();
            let symbol_name_copy = symbol_name.to_string();
            let symbol_name_start = rendered_strings.len();
            println!("\t{symbol_name} in strtab @ {symbol_name_start:x}");
            let symbol_name_c_str = CString::new(symbol_name).unwrap();
            let mut symbol_name_bytes = symbol_name_c_str.into_bytes_with_nul();
            self.names_lookup.borrow_mut().insert(symbol_name_copy, symbol_name_start);
            rendered_strings.append(&mut symbol_name_bytes);
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        self.rendered_strings.borrow().clone()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::StringsTable
    }
}

pub fn render_elf(layout: &FileLayout, data_packer: &Rc<RefCell<DataPacker>>, instruction_packer: &Rc<InstructionPacker>) -> Vec<u8> {
    // File header
    layout.append(&(Rc::new(MagicElfHeader64::new()) as Rc<dyn MagicPackable>));

    // Segments
    layout.append_segment_header(Rc::new(MagicSegmentHeader64::text_segment_header()) as Rc<dyn MagicPackable>);

    // Sections
    layout.append_section_header(Rc::new(MagicElfSection64::null_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.append_section_header(Rc::new(MagicElfSection64::text_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.append_section_header(Rc::new(MagicElfSection64::section_header_names_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.append_section_header(Rc::new(MagicElfSection64::symbol_table_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.append_section_header(Rc::new(MagicElfSection64::string_table_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.append_section_header(Rc::new(MagicElfSection64::read_only_data_section_header()) as Rc<dyn MagicSectionHeader>);
    layout.set_section_header_names_string_table(&Rc::new(MagicSectionHeaderNamesStringsTable::new()));

    // Symbols
    layout.set_symbol_table(&Rc::new(MagicSymbolTable::new(&data_packer, &instruction_packer)));
    // TODO(PT): prerender() might not be the right API:
    // it doesn't solve that the string table needs to be appended after the symbol table.
    // Otherwise, StringTable.prerender() will be called before SymbolTable.prerender(), and the string table
    // will render no strings.
    // Maybe we could have a Packable.requires_rendered(Vec<RebaseTarget>) API that then does the solving to figure
    // out the render order.
    layout.set_string_table(&Rc::new(MagicStringTable::new()));

    // Read-only data
    // TODO(PT): Similarly, instructions reference things in read-only data, which can cause a call to instructions.len()
    // during instruction rendering (crash due to borrowing im/mutable access to instructions at the same time).
    // Placing ROData prior to instructions means that when resolving pointers to ROData, we won't call instructions.len()
    layout.append(&(Rc::new(ReadOnlyData::new(&data_packer)) as Rc<dyn MagicPackable>));
    // Instructions
    layout.append(&(Rc::clone(instruction_packer) as Rc<dyn MagicPackable>));

    layout.render()
}
