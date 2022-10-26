use compilation_definitions::asm::{AsmExpr, SymbolExprOperand};
use crate::{
    assembly_packer::PotentialLabelTargetId,
    assembly_parser::{BinarySection, EquExpression, EquExpressions, Label, Labels, PotentialLabelTargets},
    println,
};
use alloc::vec::Vec;
use alloc::{
    borrow::ToOwned,
    collections::BTreeMap,
    rc::Rc,
    string::{String, ToString},
    vec,
};
use core::{cell::RefCell, mem};
use cstr_core::CString;

use crate::records::{
    any_as_u8_slice, ElfHeader64, ElfSection64, ElfSectionAttrFlag, ElfSectionType2, ElfSegment64, ElfSegmentFlag, ElfSegmentType, ElfSymbol64,
};

pub struct FileLayout {
    virtual_base: u64,
    contents: RefCell<Vec<Rc<dyn MagicPackable>>>,
    segment_headers: RefCell<Vec<Rc<dyn MagicPackable>>>,
    section_headers: RefCell<Vec<Rc<dyn MagicSectionHeader>>>,
    section_header_names_string_table: RefCell<Option<Rc<MagicSectionHeaderNamesStringsTable>>>,
    symbol_table: RefCell<Option<Rc<NewSymbolTable>>>,
    string_table: RefCell<Option<Rc<NewStringTable>>>,
    main_contents_packer: RefCell<Option<Rc<MainContentsPacker>>>,
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
            main_contents_packer: RefCell::new(None),
        }
    }

    fn set_section_header_names_string_table(&self, section_header_names_string_table: &Rc<MagicSectionHeaderNamesStringsTable>) {
        let mut strtab = self.section_header_names_string_table.borrow_mut();
        assert!(strtab.is_none(), "Cannot set section header names string table twice");
        *strtab = Some(Rc::clone(section_header_names_string_table));
        self.append(&(Rc::clone(section_header_names_string_table) as Rc<dyn MagicPackable>));
    }

    fn set_string_table(&self, string_table: &Rc<NewStringTable>) {
        let mut strtab = self.string_table.borrow_mut();
        assert!(strtab.is_none(), "Cannot set string table twice");
        *strtab = Some(Rc::clone(string_table));
        self.append(&(Rc::clone(string_table) as Rc<dyn MagicPackable>));
    }

    fn set_symbol_table(&self, symbol_table: &Rc<NewSymbolTable>) {
        let mut symtab = self.symbol_table.borrow_mut();
        assert!(symtab.is_none(), "Cannot set symbol table twice");
        *symtab = Some(Rc::clone(symbol_table));
        self.append(&(Rc::clone(symbol_table) as Rc<dyn MagicPackable>));
    }

    fn set_main_contents_packer(&self, main_contents_packer: &Rc<MainContentsPacker>) {
        let mut maybe_main_contents_packer = self.main_contents_packer.borrow_mut();
        assert!(maybe_main_contents_packer.is_none(), "Cannot set main contents packer twice");
        *maybe_main_contents_packer = Some(Rc::clone(main_contents_packer));
    }

    fn main_section_index(&self, section: BinarySection) -> usize {
        match section {
            BinarySection::Text => self.section_header_index(SectionHeaderType::TextSection),
            BinarySection::ReadOnlyData => self.section_header_index(SectionHeaderType::ReadOnlyDataSection),
        }
    }

    fn render(&self) -> Vec<u8> {
        println!("Rendering ELF");
        let contents = self.contents.borrow();

        for packable in contents.iter() {
            packable.prerender(self);
        }

        let mut out = vec![0; 0];
        for packable in contents.iter() {
            // Render the packable to bytes
            let mut rendered_packable = packable.render(self);

            let start = out.len();
            out.append(&mut rendered_packable);
            let end = out.len();

            println!("Rendered {start:016x}..{end:016x}");
        }

        let main_contents_ref = self.main_contents_packer.borrow();
        let main_contents = main_contents_ref.as_ref().unwrap();
        let mut rendered_main_contents = main_contents.render(self);
        out.append(&mut rendered_main_contents);

        out
    }

    pub fn address_of_label(&self, label: &Label) -> usize {
        let main_contents_ref = self.main_contents_packer.borrow();
        let main_contents = main_contents_ref.as_ref().unwrap();
        let end_of_packables = {
            let contents = self.contents.borrow();
            contents.iter().fold(0, |acc, p| acc + p.len())
        };
        (self.virtual_base as usize) + end_of_packables + main_contents.offset_of_label(label)
    }

    pub fn address_of_label_name(&self, label_name: &str) -> usize {
        let main_contents_ref = self.main_contents_packer.borrow();
        let main_contents = main_contents_ref.as_ref().unwrap();
        let end_of_packables = {
            let contents = self.contents.borrow();
            contents.iter().fold(0, |acc, p| acc + p.len())
        };
        (self.virtual_base as usize) + end_of_packables + main_contents.offset_of_label_name(label_name)
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
        match target {
            RebaseTarget::TextSegment | RebaseTarget::TextSection | RebaseTarget::ReadOnlyDataSection => {
                // Add up the total header size
                let contents = self.contents.borrow();
                let total_header_len = contents.iter().fold(0, |acc, packable| acc + packable.len());
                if target == RebaseTarget::TextSegment {
                    total_header_len
                } else {
                    // Find the offset of the specified section
                    let main_contents_ref = self.main_contents_packer.borrow();
                    let main_contents = main_contents_ref.as_ref().unwrap();
                    let translated_target = match target {
                        RebaseTarget::TextSection => BinarySection::Text,
                        RebaseTarget::ReadOnlyDataSection => BinarySection::ReadOnlyData,
                        _ => panic!("Shouldn't happen"),
                    };
                    total_header_len + main_contents.offset_of_section_start(translated_target)
                }
            }
            RebaseTarget::ElfHeader
            | RebaseTarget::ElfSegmentHeader
            | RebaseTarget::ElfSectionHeader
            | RebaseTarget::SymbolTableSection
            | RebaseTarget::StringsTable
            | RebaseTarget::SectionHeadersStringsTable => {
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
        }
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

    fn target_len(&self, target: RebaseTarget) -> usize {
        match target {
            RebaseTarget::TextSegment => {
                // TODO(PT): This will need to change when we also have a .data segment
                self.target_len(RebaseTarget::TextSection) + self.target_len(RebaseTarget::ReadOnlyDataSection)
            }
            RebaseTarget::TextSection | RebaseTarget::ReadOnlyDataSection => {
                let main_contents_ref = self.main_contents_packer.borrow();
                let main_contents = main_contents_ref.as_ref().unwrap();
                let translated_target = match target {
                    RebaseTarget::TextSection => BinarySection::Text,
                    RebaseTarget::ReadOnlyDataSection => BinarySection::ReadOnlyData,
                    _ => panic!("Shouldn't happen"),
                };
                main_contents.section_len(translated_target)
            }
            RebaseTarget::ElfHeader
            | RebaseTarget::ElfSegmentHeader
            | RebaseTarget::ElfSectionHeader
            | RebaseTarget::SymbolTableSection
            | RebaseTarget::StringsTable
            | RebaseTarget::SectionHeadersStringsTable => {
                let target_packable = self.get_target(target);
                target_packable.len()
            }
        }
    }

    fn static_end_of(&self, target: RebaseTarget) -> usize {
        self.static_start_of(target) + self.target_len(target)
    }

    fn virt_start_of(&self, target: RebaseTarget) -> usize {
        self.static_start_of(target) + (self.virtual_base as usize)
    }

    fn get_target(&self, target: RebaseTarget) -> Rc<dyn MagicPackable> {
        let contents = self.contents.borrow();
        let target_packables: Vec<&Rc<dyn MagicPackable>> = contents.iter().filter(|p| p.struct_type() == target).collect();
        assert!(
            target_packables.len() == 1,
            "Expected exactly one struct with the provided type {target:?}, got {}",
            target_packables.len()
        );
        let target_packable = target_packables[0];
        Rc::clone(target_packable)
    }

    fn get_rebased_section_header_name_offset(&self, section_header: &dyn MagicSectionHeader) -> usize {
        let maybe_strtab = self.section_header_names_string_table.borrow();
        match &*maybe_strtab {
            Some(strtab) => strtab.offset_of_name(section_header.name()),
            None => panic!("Expected a section header names strings table to be set up"),
        }
    }

    fn get_rebased_string_offset_in_strtab(&self, symbol_name: &str) -> usize {
        let maybe_strtab = self.string_table.borrow();
        match &*maybe_strtab {
            Some(strtab) => strtab.offset_of_string(symbol_name),
            None => panic!("Expected a string table to be set up"),
        }
    }

    fn section_header_index(&self, section_header_type: SectionHeaderType) -> usize {
        let section_headers = self.section_headers.borrow();
        let section_header_indexes_matching_type: Vec<usize> = section_headers
            .iter()
            .enumerate()
            // Filter to the desired section type
            .filter(|(_i, sh)| sh.section_header_type() == section_header_type)
            // Only retain the index
            .map(|(i, _sh)| i)
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
        self.virt_start_of(target) + offset
    }

    pub fn get_rebased_value(&self, rebased_value: RebasedValue) -> usize {
        match rebased_value {
            RebasedValue::VirtStartOf(target) => self.virt_start_of(target),
            RebasedValue::StaticStartOf(target) => self.static_start_of(target),
            RebasedValue::StaticEndOf(target) => self.static_end_of(target),
            RebasedValue::_VirtOffsetWithin(target, offset) => self.virt_offset_within(target, offset),
            RebasedValue::SectionHeaderIndex(section_header_type) => self.section_header_index(section_header_type),
            RebasedValue::SectionHeaderCount => self.section_headers.borrow().len(),
            RebasedValue::SizeOf(target) => self.target_len(target),
            RebasedValue::_SectionHeaderName => panic!("Use get_rebased_section_header_name_offset"),
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
        }
    }

    pub fn distance_between_atom_id_and_label_name(&self, atom_id: PotentialLabelTargetId, label_name: &str) -> isize {
        let maybe_main_contents = self.main_contents_packer.borrow();
        let main_contents = maybe_main_contents.as_ref().unwrap();
        main_contents.distance_between_atom_id_and_label_name(atom_id, label_name)
    }

    pub fn symbol_type(&self, symbol_name: &str) -> SymbolEntryType {
        let maybe_main_contents = self.main_contents_packer.borrow();
        let main_contents = maybe_main_contents.as_ref().unwrap();
        main_contents.symbol_type(symbol_name)
    }

    pub fn value_of_symbol(&self, symbol_name: &str) -> usize {
        let maybe_main_contents = self.main_contents_packer.borrow();
        let main_contents = maybe_main_contents.as_ref().unwrap();
        main_contents.value_of_symbol(symbol_name)
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum RebaseTarget {
    ElfHeader,
    ElfSegmentHeader,
    ElfSectionHeader,
    TextSegment,
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
    _VirtOffsetWithin(RebaseTarget, usize),
    SectionHeaderIndex(SectionHeaderType),
    SizeOf(RebaseTarget),
    Literal(usize),
    VirtualBase,
    SegmentHeadersHead,
    SegmentHeaderCount,
    SectionHeadersHead,
    SectionHeaderCount,
    // Only for MagicSectionHeader
    _SectionHeaderName,
}

pub trait MagicPackable {
    fn len(&self) -> usize;
    // PT: To remove?
    fn struct_type(&self) -> RebaseTarget;
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
    fn prerender(&self, _layout: &FileLayout) {}
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
            RebasedValue::StaticEndOf(RebaseTarget::TextSegment),
            RebasedValue::StaticEndOf(RebaseTarget::TextSegment),
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
        let out = ElfSegment64 {
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
        self.rendered_strings.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
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

    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        self.rendered_strings.borrow().clone()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::SectionHeadersStringsTable
    }
}

#[derive(Debug)]
pub enum SymbolEntryType {
    SymbolWithBackingData,
    SymbolWithInlinedValue,
}

struct MainContentsPacker {
    main_contents: MainContentsDescription,
}

impl MainContentsPacker {
    fn new(main_contents: MainContentsDescription) -> Rc<Self> {
        Rc::new(Self { main_contents })
    }

    fn distance_between_atom_id_and_label_name(&self, atom_id: PotentialLabelTargetId, label_name: &str) -> isize {
        self.main_contents.distance_between_atom_id_and_label_name(atom_id, label_name)
    }

    fn offset_of_label_name(&self, label_name: &str) -> usize {
        self.main_contents.offset_of_label_name(label_name)
    }

    fn offset_of_label(&self, label: &Label) -> usize {
        self.main_contents.offset_of_label(label)
    }

    fn offset_of_section_start(&self, section: BinarySection) -> usize {
        let mut offset = 0;
        for atom in self.main_contents.atoms.0.iter() {
            if atom.container_section() == section {
                return offset;
            }
            offset += atom.len();
        }
        panic!("Failed to find section {section}")
    }

    fn section_len(&self, section: BinarySection) -> usize {
        let mut len = 0;
        for atom in self.main_contents.atoms.0.iter() {
            // Traverse to the first atom of the section
            if atom.container_section() != section {
                continue;
            }
            len += atom.len();
        }
        len
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = vec![];
        for atom in self.main_contents.atoms.0.iter() {
            let mut rendered_atom = atom.render(layout);
            // Sanity check
            assert_eq!(rendered_atom.len(), atom.len(), "Rendered atom was a different length from what it claimed: {atom}");
            out.append(&mut rendered_atom)
        }
        out
    }

    fn symbol_type(&self, symbol_name: &str) -> SymbolEntryType {
        self.main_contents.symbol_type(symbol_name)
    }

    fn value_of_symbol(&self, symbol_name: &str) -> usize {
        self.main_contents.value_of_symbol_name(symbol_name)
    }
}

struct NewSymbolTable {
    main_contents: MainContentsDescription,
}

impl NewSymbolTable {
    fn new(main_contents: MainContentsDescription) -> Self {
        Self { main_contents }
    }
}

impl MagicPackable for NewSymbolTable {
    fn len(&self) -> usize {
        // Add one for the null symbol
        let symbol_count = 1 + (self.main_contents.labels.0.len() + self.main_contents.equ_expressions.0.len());
        symbol_count * mem::size_of::<ElfSymbol64>()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::SymbolTableSection
    }

    fn prerender(&self, _layout: &FileLayout) {}

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = vec![];

        // Render the null symbol
        let null_symbol = ElfSymbol64 {
            // The NULL symbol will always take the first byte of the string table,
            // so it's fine to hard-code this
            name: 0,
            info: 0,
            other: 0,
            owner_section_index: 0,
            value: 0,
            size: 0,
        };
        let mut null_symbol_bytes = unsafe { any_as_u8_slice(&null_symbol) }.to_owned();
        out.append(&mut null_symbol_bytes);

        for label in self.main_contents.labels.0.iter() {
            let maybe_data_unit = label.data_unit.borrow();
            let data_unit = maybe_data_unit.as_ref().unwrap();
            let container_section = data_unit.container_section();
            let container_section_idx = layout.main_section_index(container_section);

            let rendered_symbol = ElfSymbol64 {
                name: layout.get_rebased_string_offset_in_strtab(&label.name) as _,
                info: 0,
                other: 0,
                owner_section_index: container_section_idx as _,
                value: layout.address_of_label(label) as _,
                size: 0,
            };
            let mut rendered_symbol_bytes = unsafe { any_as_u8_slice(&rendered_symbol) }.to_owned();
            out.append(&mut rendered_symbol_bytes);
        }

        for equ_expr in self.main_contents.equ_expressions.0.iter() {
            // We need the previous atom to compute the current offset
            let rendered_symbol = ElfSymbol64 {
                name: layout.get_rebased_string_offset_in_strtab(&equ_expr.name) as _,
                info: 0,
                other: 0,
                // Special section for 'absolute' symbols defined by the ELF spec
                owner_section_index: 0xfff1,
                value: self.main_contents.evaluate_equ(equ_expr) as _,
                size: 0,
            };
            let mut rendered_symbol_bytes = unsafe { any_as_u8_slice(&rendered_symbol) }.to_owned();
            out.append(&mut rendered_symbol_bytes);
        }

        out
    }
}

pub struct NewStringTable {
    rendered_strings: Vec<u8>,
    name_to_offset_lookup: BTreeMap<String, usize>,
}

impl NewStringTable {
    pub fn new(main_contents: MainContentsDescription) -> Rc<Self> {
        let mut rendered_strings = vec![];
        let mut name_to_offset_lookup = BTreeMap::new();

        // Render the null string
        rendered_strings.push(0x00);

        for label in main_contents.labels.0.iter() {
            let symbol_name = label.name.to_string();
            let symbol_name_c_str = CString::new(symbol_name.clone()).unwrap();
            let mut symbol_name_bytes = symbol_name_c_str.into_bytes_with_nul();
            name_to_offset_lookup.insert(symbol_name, rendered_strings.len());
            rendered_strings.append(&mut symbol_name_bytes);
        }
        for equ_expr in main_contents.equ_expressions.0.iter() {
            let equ_name = equ_expr.name.to_string();
            let equ_name_c_str = CString::new(equ_name.clone()).unwrap();
            let mut equ_name_bytes = equ_name_c_str.into_bytes_with_nul();
            name_to_offset_lookup.insert(equ_name, rendered_strings.len());
            rendered_strings.append(&mut equ_name_bytes);
        }
        Rc::new(Self {
            rendered_strings,
            name_to_offset_lookup,
        })
    }

    fn offset_of_string(&self, string: &str) -> usize {
        *self.name_to_offset_lookup.get(string).unwrap()
    }
}

impl MagicPackable for NewStringTable {
    fn len(&self) -> usize {
        self.rendered_strings.len()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::StringsTable
    }

    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        self.rendered_strings.clone()
    }
}

#[derive(Clone)]
pub struct MainContentsDescription {
    labels: Labels,
    equ_expressions: EquExpressions,
    atoms: PotentialLabelTargets,
}

impl MainContentsDescription {
    fn new(labels: Labels, equ_expressions: EquExpressions, atoms: PotentialLabelTargets) -> Self {
        Self {
            labels,
            equ_expressions,
            atoms,
        }
    }

    pub fn symbol_type(&self, symbol_name: &str) -> SymbolEntryType {
        for label in self.labels.0.iter() {
            if label.name == symbol_name {
                return SymbolEntryType::SymbolWithBackingData;
            }
        }
        for equ_expr in self.equ_expressions.0.iter() {
            if equ_expr.name == symbol_name {
                return SymbolEntryType::SymbolWithInlinedValue;
            }
        }
        panic!("Failed to find symbol named {symbol_name}");
    }

    pub fn offset_of_atom_id(&self, atom_id: PotentialLabelTargetId) -> usize {
        let mut offset = 0;
        for a in self.atoms.0.iter() {
            if a.id() == atom_id {
                return offset;
            }
            offset += a.len();
        }
        panic!("Failed to find atom with ID {atom_id}");
    }

    fn distance_between_atom_id_and_label_name(&self, atom_id: PotentialLabelTargetId, label_name: &str) -> isize {
        // TODO(PT): Disallow this for .equ? Check whether jmp can target an .equ label (probably not!)
        let offset_of_label = self.offset_of_label_name(label_name);
        let offset_of_atom = self.offset_of_atom_id(atom_id);
        (offset_of_label as isize) - (offset_of_atom as isize)
    }

    fn offset_of_label(&self, label: &Label) -> usize {
        // There is a label with this name
        let maybe_label_atom = label.data_unit.borrow();
        let label_atom = maybe_label_atom.as_ref().unwrap();
        let mut offset = 0;
        for atom in self.atoms.0.iter() {
            if Rc::ptr_eq(atom, label_atom) {
                return offset;
            }
            offset += atom.len();
        }
        panic!("Failed to find atom attached to label");
    }

    fn offset_of_label_name(&self, label_name: &str) -> usize {
        let label = self.labels.0.iter().find(|label| label.name == label_name);
        if let Some(label) = label {
            return self.offset_of_label(label);
        }
        panic!("Failed to find a label named {label_name}");
    }

    pub fn evaluate_equ(&self, equ_expression: &EquExpression) -> usize {
        let maybe_previous_atom = equ_expression.previous_data_unit.borrow();
        let previous_atom = maybe_previous_atom.as_ref().unwrap();
        let get_op_value = move |op: &SymbolExprOperand| match op {
            SymbolExprOperand::OutputCursor => self.offset_of_atom_id(previous_atom.id()) + previous_atom.len(),
            SymbolExprOperand::StartOfSymbol(label_name) => self.offset_of_label_name(label_name),
        };

        let value = match &equ_expression.expression {
            AsmExpr::Subtract(op1, op2) => get_op_value(op1) - get_op_value(op2),
        };
        value
    }

    pub fn value_of_symbol_name(&self, label_name: &str) -> usize {
        let label = self.labels.0.iter().find(|label| label.name == label_name);
        if let Some(label) = label {
            return self.offset_of_label(label);
        } else {
            // Search for an .equ statement with this name
            let equ_expression = self.equ_expressions.0.iter().find(|equ| equ.name == label_name).unwrap();
            return self.evaluate_equ(&equ_expression);
        }
    }
}

pub fn render_elf(layout: &FileLayout, labels: Labels, equ_expressions: EquExpressions, atoms: PotentialLabelTargets) -> Vec<u8> {
    let main_contents_desc = MainContentsDescription::new(labels.clone(), equ_expressions.clone(), atoms.clone());
    let main_contents = MainContentsPacker::new(main_contents_desc.clone());
    layout.set_main_contents_packer(&main_contents);

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

    // Only add a section header for .rodata if the assembled code requires one
    let needs_rodata = {
        let mut contains_rodata = false;
        for atom in atoms.0.iter() {
            if atom.container_section() == BinarySection::ReadOnlyData {
                contains_rodata = true;
                break;
            }
        }
        contains_rodata
    };
    if needs_rodata {
        layout.append_section_header(Rc::new(MagicElfSection64::read_only_data_section_header()) as Rc<dyn MagicSectionHeader>);
    }
    layout.set_section_header_names_string_table(&Rc::new(MagicSectionHeaderNamesStringsTable::new()));

    // Symbols
    layout.set_symbol_table(&Rc::new(NewSymbolTable::new(main_contents_desc.clone())));
    layout.set_string_table(&NewStringTable::new(main_contents_desc.clone()));

    // TODO(PT): Might need an OwnerSegment for each section
    // This will allow us to construct the proper load commands/proper sizes

    layout.render()
}
