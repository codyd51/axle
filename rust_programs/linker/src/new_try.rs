use alloc::{borrow::ToOwned, collections::BTreeMap, rc::Rc, string::ToString, vec};
use alloc::{slice, vec::Vec};
use bitflags::bitflags;
use core::{cell::RefCell, fmt::Display, mem, ops::Index};
use cstr_core::CString;
use std::println;

use crate::records::{
    any_as_u8_slice, ElfHeader64, ElfHeader64Record, ElfSection64, ElfSection64Record, ElfSectionAttrFlag, ElfSectionType, ElfSectionType2, ElfSegment64,
    ElfSegment64Record, ElfSegmentFlag, ElfSegmentType, ElfSymbol64, ElfSymbol64Record, Packable, StringTableHelper, SymbolTable, VecRecord,
};

#[derive(PartialEq)]
struct DataSymbol {
    name: String,
    inner: Vec<u8>,
}

impl DataSymbol {
    fn new(name: &str, inner: Vec<u8>) -> Self {
        Self { name: name.to_string(), inner }
    }
}

impl Display for DataSymbol {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("<DataSymbol \"{}\" ({} bytes)>", self.name, self.inner.len()))
    }
}

struct MoveDataSymbolToRegister {
    dest_register: String,
    symbol: Rc<DataSymbol>,
}

impl MoveDataSymbolToRegister {
    fn new(dest_register: &str, symbol: &Rc<DataSymbol>) -> Self {
        Self {
            dest_register: dest_register.to_string(),
            symbol: Rc::clone(symbol),
        }
    }
}

struct Function {
    name: String,
    instructions: Vec<MoveDataSymbolToRegister>,
}

impl Function {
    fn new(name: &str, instructions: Vec<MoveDataSymbolToRegister>) -> Self {
        Self {
            name: name.to_string(),
            instructions,
        }
    }
}

struct DataPacker {
    file_layout: Rc<FileLayout>,
    symbols: Vec<(usize, Rc<DataSymbol>)>,
}

impl DataPacker {
    fn new(file_layout: &Rc<FileLayout>) -> Self {
        Self {
            file_layout: Rc::clone(file_layout),
            symbols: Vec::new(),
        }
    }

    fn total_size(&self) -> usize {
        /*
        let mut cursor = 0;
        for sym in self.symbols.iter() {
            cursor += sym.inner.len();
        }
        cursor
        */
        let last_entry = self.symbols.last();
        match last_entry {
            None => 0,
            Some(last_entry) => last_entry.0 + last_entry.1.inner.len(),
        }
    }

    pub fn pack(self_: &Rc<RefCell<Self>>, sym: &Rc<DataSymbol>) {
        let mut this = self_.borrow_mut();
        let total_size = this.total_size();
        println!("Data packer tracking {sym}");
        this.symbols.push((total_size, Rc::clone(sym)));
    }

    pub fn offset_of(self_: &Rc<RefCell<Self>>, sym: &Rc<DataSymbol>) -> usize {
        let this = self_.borrow();
        for (offset, s) in this.symbols.iter() {
            if *s == *sym {
                return *offset;
            }
        }
        panic!("Failed to find symbol {sym}")
    }

    /*
    pub fn render(self: &Rc<Self>) -> Vec<u8> {
        let len = self.total_size();
        let mut out = vec![0; len];
        let mut cursor = 0;
        for sym in self.symbols.iter() {
            out[cursor..cursor + sym.inner.len()].copy_from_slice(&sym.inner);
        }
        out
    }
    */
}

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
        for (_, symbol) in data_packer.symbols.iter() {
            let mut symbol_bytes = symbol.inner.to_owned();
            rendered_data.append(&mut symbol_bytes);
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        self.rendered_data.borrow().to_owned()
    }
}

struct InstructionPacker {
    file_layout: Rc<FileLayout>,
    data_packer: Rc<RefCell<DataPacker>>,
    instructions: RefCell<Vec<(Rc<MoveDataSymbolToRegister>, RebasedValue)>>,
}

impl InstructionPacker {
    fn new(file_layout: &Rc<FileLayout>, data_packer: &Rc<RefCell<DataPacker>>) -> Self {
        Self {
            file_layout: Rc::clone(file_layout),
            data_packer: Rc::clone(data_packer),
            instructions: RefCell::new(Vec::new()),
        }
    }

    fn pack(&self, instr: &Rc<MoveDataSymbolToRegister>) {
        let symbol_offset = DataPacker::offset_of(&self.data_packer, &instr.symbol);
        let rel_ptr = RebasedValue::OffsetWithin(RebaseTarget::ReadOnlyDataSection, symbol_offset);
        let mut instructions = self.instructions.borrow_mut();
        instructions.push((Rc::clone(instr), rel_ptr));
    }
}

impl MagicPackable for InstructionPacker {
    fn len(&self) -> usize {
        let mut len = 0;
        let instructions = self.instructions.borrow();
        for instr in instructions.iter() {
            //len += instr.len();
            len += 4;
        }
        len
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = vec![];
        let instructions = self.instructions.borrow();
        for instr in instructions.iter() {
            // PT matches the len above
            let mut instruction_bytes = CString::new("Test").unwrap().into_bytes();
            out.append(&mut instruction_bytes);
        }
        out
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::TextSection
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
    fn new(virtual_base: u64) -> Self {
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
        println!("Render");
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
            //out[out.len()..out.len() + rendered_packable.len()].copy_from_slice(&rendered_packable);
            let end = out.len();

            println!("Rendered Packable to {} bytes", end - start);
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

    fn get_rebased_value(&self, rebased_value: RebasedValue) -> usize {
        match rebased_value {
            RebasedValue::VirtStartOf(target) => self.virt_start_of(target),
            RebasedValue::StaticStartOf(target) => self.static_start_of(target),
            RebasedValue::StaticEndOf(target) => self.static_end_of(target),
            RebasedValue::OffsetWithin(target, offset) => todo!(),
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
        }
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
    OffsetWithin(RebaseTarget, usize),
    SectionHeaderIndex(SectionHeaderType),
    SizeOf(RebaseTarget),
    Literal(usize),
    VirtualBase,
    SegmentHeadersHead,
    SegmentHeaderCount,
    SectionHeadersHead,
    SectionHeaderCount,
    // Only for MagicSectionHeader
    SectionHeaderName,
}

trait MagicPackable {
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

/*
trait MagicSectionHeaderNameLookup: MagicPackable {
    fn offset_of_name_for_section_header(&self, section_header: &Rc<dyn MagicSectionHeader>);
}
*/

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
            //align: 0x0000000000200000,
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
        println!("Len {}", self.rendered_strings.borrow().len());
        self.rendered_strings.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
        // Perhaps we need a .strings()
        // For section headers it'd just be 1 string at index 0
        // For symbols it'd be more
        println!("Inside prerender");
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

#[derive(Debug, Copy, Clone)]
pub enum SymbolType {
    NullSymbol,
    DataSymbol,
    CodeSymbol,
}

struct MagicElfSymbol64 {
    pub symbol_type: SymbolType,
    pub name: String,
}

impl MagicElfSymbol64 {
    fn new(symbol_type: SymbolType, name: &str) -> Self {
        Self {
            symbol_type,
            name: name.to_string(),
        }
    }
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
}

impl MagicPackable for MagicSymbolTable {
    fn len(&self) -> usize {
        self.symbols.borrow().len() * mem::size_of::<ElfSymbol64>()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::SymbolTableSection
    }

    fn prerender(&self, layout: &FileLayout) {
        println!("Pre-rendering symbol table!");
        let data_packer = self.data_packer.borrow();
        let mut symbols = self.symbols.borrow_mut();

        // Write the null symbol
        symbols.push(MagicElfSymbol64::new(SymbolType::NullSymbol, ""));

        for (_, data_symbol) in data_packer.symbols.iter() {
            symbols.push(MagicElfSymbol64::new(SymbolType::DataSymbol, &data_symbol.name));
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        println!("Rendering symbol table");
        let mut out = vec![];
        let symbols = self.symbols.borrow();
        for (i, symbol) in symbols.iter().enumerate() {
            let rendered_symbol = ElfSymbol64 {
                name: layout.get_rebased_symbol_name_offset(symbol) as _,
                info: 0 as _,
                other: 0 as _,
                owner_section_index: layout.get_rebased_symbol_owner_section_index(symbol) as _,
                value: 0 as _,
                size: 0 as _,
                //value: i as _,
                //size: i as _,
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
        println!("Len of string table {}", self.rendered_strings.borrow().len());
        self.rendered_strings.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
        println!("Inside prerender for string table");
        let mut rendered_strings = self.rendered_strings.borrow_mut();
        //let symbol_table = &layout.symbol_table.borrow().unwrap();
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

pub fn pack_elf2() -> Vec<u8> {
    let mut data = BTreeMap::new();

    let sym1 = Rc::new(DataSymbol::new("word1", CString::new("Hello ").unwrap().into_bytes_with_nul()));
    data.insert(sym1.name.clone(), Rc::clone(&sym1));
    let sym2 = Rc::new(DataSymbol::new("word2", CString::new(" world!\n").unwrap().into_bytes_with_nul()));
    data.insert(sym2.name.clone(), Rc::clone(&sym2));

    let instructions = [
        Rc::new(MoveDataSymbolToRegister::new("rcx", data.get(&sym1.name).unwrap())),
        Rc::new(MoveDataSymbolToRegister::new("rax", data.get(&sym2.name).unwrap())),
    ];

    let layout = Rc::new(FileLayout::new(0x400000));

    // Render the data symbols
    let mut data_packer = Rc::new(RefCell::new(DataPacker::new(&layout)));
    for data_sym in data.values() {
        DataPacker::pack(&data_packer, &data_sym);
    }

    // Render the instructions
    let mut instruction_packer = Rc::new(InstructionPacker::new(&layout, &data_packer));
    for instr in instructions.iter() {
        instruction_packer.pack(instr);
    }

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
    //layout.append_symbol_table(&Rc::new(MagicSymbolTable::new(&data_packer, &instruction_packer)));
    layout.set_symbol_table(&Rc::new(MagicSymbolTable::new(&data_packer, &instruction_packer)));
    //layout.append(&(Rc::new(MagicStringTable::new()) as Rc<dyn MagicPackable>));
    layout.set_string_table(&Rc::new(MagicStringTable::new()));
    //layout.append(

    // Read-only data
    layout.append(&(Rc::new(ReadOnlyData::new(&data_packer)) as Rc<dyn MagicPackable>));

    // Instructions
    layout.append(&(instruction_packer as Rc<dyn MagicPackable>));

    layout.render()

    // Onion packing:
    // First, lay out the constant data
    // Then, lay out the string table
    // Then, lay out the symbol table
    // Then, lay out the string table header
    // Then, lay out the symbol table header
    // ...
    // But, the code genuinely needs to know the final location of the data, to be assembled
    // Do layout, then do rendering - requires that encoded code size won't change
    //  Maybe we *can* make this guarantee?

    // First things first, we'll need to

    /*
    // Executable code
    let code = vec![
        //  mov rax, 0xc
        0x48, 0xC7, 0xC0, 0x0C, 0x00, 0x00, 0x00, // mov rbx, 0x1
        0x48, 0xC7, 0xC3, 0x01, 0x00, 0x00, 0x00, // mov rcx, 0x4000a2
        0x48, 0xC7, 0xC1, 0xA2, 0x00, 0x40, 0x00, // mov rdx, 0xd
        0x48, 0xC7, 0xC2, 0x0D, 0x00, 0x00, 0x00, // int 0x80
        0xCD, 0x80, // mov rbx, rax
        0x48, 0x89, 0xC3, // mov rax, 0xd
        0x48, 0xC7, 0xC0, 0x0D, 0x00, 0x00, 0x00, // int 0x80
        0xCD, 0x80,
    ];
    let string = DataSymbol::new("msg", vec!['H' as u8, 'e' as u8, 'l' as u8, 'l' as u8, 'o' as u8, 0]);
    */

    /*
    let _start = Function("\xab");

    let symtab = Symtab(
        Symbol(
            "_start",
            _start,
        )
    );

    elf.set_symtab(symtab)

    render {
        // Create string table
        let strtab = Strtab()
        for symbol in symbols {
            strtab.append(symbol.name)
            symbol.name = strtab.offset(symbol)
        }

        let rendered_strtab = strtab.render();
        let rendered_symtab = symtab.render();
    }
    */
    //Vec::new()
}
