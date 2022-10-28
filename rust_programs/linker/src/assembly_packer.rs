use alloc::vec::Vec;
use alloc::{borrow::ToOwned, rc::Rc, string::String, vec};
use core::{
    fmt::{Debug, Display},
    mem,
};

#[cfg(feature = "run_in_axle")]
use axle_rt::println;
use compilation_definitions::encoding::{ModRmAddressingMode, ModRmByte, RexPrefix};
use compilation_definitions::instructions::Instr;
#[cfg(not(feature = "run_in_axle"))]
use std::{print, println};

use compilation_definitions::prelude::*;

use crate::{
    assembly_lexer::AssemblyLexer,
    assembly_parser::{AssemblyParser, BinarySection, EquExpressions, Labels, PotentialLabelTargets},
    new_try::{FileLayout, SymbolEntryType},
};

#[derive(Debug, Clone)]
pub enum DataSource {
    Literal(usize),
    NamedDataSymbol(String),
    RegisterContents(RegView),
}

static mut NEXT_ATOM_ID: usize = 0;

pub fn next_atom_id() -> PotentialLabelTargetId {
    unsafe {
        let ret = NEXT_ATOM_ID;
        NEXT_ATOM_ID += 1;
        PotentialLabelTargetId(ret)
    }
}

#[derive(Debug)]
pub struct InstrDataUnit {
    id: PotentialLabelTargetId,
    instr: Instr,
}

impl InstrDataUnit {
    pub fn new(instr: &Instr) -> Self {
        Self {
            id: next_atom_id(),
            instr: instr.clone(),
        }
    }
}

impl Instruction for InstrDataUnit {
    // TODO(PT): Update this to handle different register view sizes
    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        self.instr.assemble()
    }
}

impl PotentialLabelTarget for InstrDataUnit {
    fn container_section(&self) -> BinarySection {
        BinarySection::Text
    }

    fn len(&self) -> usize {
        self.instr.assembled_len()
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        Instruction::render(self, layout)
    }

    fn id(&self) -> PotentialLabelTargetId {
        self.id
    }
}

impl Display for InstrDataUnit {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("InstrDataUnit({:?})", self.instr))
    }
}

#[derive(Debug)]
pub struct Interrupt {
    id: PotentialLabelTargetId,
    vector: u8,
}

impl Interrupt {
    pub fn new(vector: u8) -> Self {
        Self { id: next_atom_id(), vector }
    }
}

impl Instruction for Interrupt {
    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        vec![
            // INT opcode
            0xcd,
            // INT vector
            self.vector,
        ]
    }
}

impl PotentialLabelTarget for Interrupt {
    fn container_section(&self) -> BinarySection {
        BinarySection::Text
    }

    fn len(&self) -> usize {
        2
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        Instruction::render(self, layout)
    }

    fn id(&self) -> PotentialLabelTargetId {
        self.id
    }
}

impl Display for Interrupt {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("int 0x{:02x}", self.vector))
    }
}

#[derive(Debug, Clone)]
pub enum JumpTarget {
    Label(String),
}

#[derive(Debug)]
pub struct Jump {
    id: PotentialLabelTargetId,
    target: JumpTarget,
}

impl Jump {
    pub fn new(target: JumpTarget) -> Self {
        Self { id: next_atom_id(), target }
    }
}

impl Instruction for Jump {
    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out: Vec<u8> = vec![];
        let JumpTarget::Label(label_name) = &self.target;
        // TODO(PT): Look for opportunities to use a JMP variant with a smaller encoded size
        // (i.e. if the named symbols is close by, use the JMP reloff8 variant)
        // This would require us to vary self.len()
        // We might have an intermediate stage that selects a more specific variant, if for example
        // we can see the jump target is only 4 atoms away
        // JMP rel32off
        let distance_to_target = layout.distance_between_atom_id_and_label_name(PotentialLabelTarget::id(self), label_name) - (self.len() as isize);
        let distance_to_target: i32 = distance_to_target.try_into().unwrap();
        out.push(0xe9);
        let mut distance_as_bytes = distance_to_target.to_le_bytes().to_vec();
        assert!(distance_as_bytes.len() <= mem::size_of::<u32>(), "Must fit in a u32");
        distance_as_bytes.resize(mem::size_of::<u32>(), 0);
        out.append(&mut distance_as_bytes);
        out
    }
}

impl PotentialLabelTarget for Jump {
    fn container_section(&self) -> BinarySection {
        BinarySection::Text
    }

    fn id(&self) -> PotentialLabelTargetId {
        self.id
    }

    fn len(&self) -> usize {
        5
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        Instruction::render(self, layout)
    }
}

impl Display for Jump {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("jmp {:?}", self.target))
    }
}

#[derive(Debug)]
pub struct MetaInstrJumpToLabelIfEqual {
    id: PotentialLabelTargetId,
    target: JumpTarget,
}

impl MetaInstrJumpToLabelIfEqual {
    pub fn new(target: JumpTarget) -> Self {
        Self { id: next_atom_id(), target }
    }
}

// TODO(PT): Replace this abstraction with a pass that iterates all the instructions and
// replaces 'meta instructions' like JumpToLabel with concrete instructions
impl Instruction for MetaInstrJumpToLabelIfEqual {
    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let JumpTarget::Label(label_name) = &self.target;
        let distance_to_target = layout.distance_between_atom_id_and_label_name(PotentialLabelTarget::id(self), label_name) - (self.len() as isize);
        let distance_to_target: i32 = distance_to_target.try_into().unwrap();
        Instr::JumpToRelOffIfEqual(distance_to_target as isize).assemble()
    }
}

impl PotentialLabelTarget for MetaInstrJumpToLabelIfEqual {
    fn container_section(&self) -> BinarySection {
        BinarySection::Text
    }

    fn id(&self) -> PotentialLabelTargetId {
        self.id
    }

    fn len(&self) -> usize {
        Instr::JumpToRelOffIfEqual(0).assembled_len()
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        Instruction::render(self, layout)
    }
}

impl Display for MetaInstrJumpToLabelIfEqual {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("je {:?}", self.target))
    }
}

#[derive(Debug)]
pub struct MetaInstrJumpToLabelIfNotEqual {
    id: PotentialLabelTargetId,
    target: JumpTarget,
}

impl MetaInstrJumpToLabelIfNotEqual {
    pub fn new(target: JumpTarget) -> Self {
        Self { id: next_atom_id(), target }
    }
}

impl Instruction for MetaInstrJumpToLabelIfNotEqual {
    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let JumpTarget::Label(label_name) = &self.target;
        let distance_to_target = layout.distance_between_atom_id_and_label_name(PotentialLabelTarget::id(self), label_name) - (self.len() as isize);
        let distance_to_target: i32 = distance_to_target.try_into().unwrap();
        Instr::JumpToRelOffIfNotEqual(distance_to_target as isize).assemble()
    }
}

impl PotentialLabelTarget for MetaInstrJumpToLabelIfNotEqual {
    fn container_section(&self) -> BinarySection {
        BinarySection::Text
    }

    fn id(&self) -> PotentialLabelTargetId {
        self.id
    }

    fn len(&self) -> usize {
        Instr::JumpToRelOffIfNotEqual(0).assembled_len()
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        Instruction::render(self, layout)
    }
}

impl Display for MetaInstrJumpToLabelIfNotEqual {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("jne {:?}", self.target))
    }
}

#[derive(Debug, PartialEq, Copy, Clone, Eq, PartialOrd, Ord)]
pub struct PotentialLabelTargetId(pub usize);

impl Display for PotentialLabelTargetId {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("AtomId({})", self.0))
    }
}

// An instruction, piece of constant data, or expression
pub trait PotentialLabelTarget: Display + Debug {
    fn container_section(&self) -> BinarySection;
    fn len(&self) -> usize;
    fn id(&self) -> PotentialLabelTargetId;
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
}

pub trait Instruction: Display + PotentialLabelTarget {
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
}

pub fn parse(_layout: &Rc<FileLayout>, source: &str) -> (Labels, EquExpressions, PotentialLabelTargets) {
    // Generate code and data from source
    let lexer = AssemblyLexer::new(source);
    let mut parser = AssemblyParser::new(lexer);
    let (labels, equ_expressions, data_units) = parser.parse();
    /*
    println!("[### Assembly + ELF rendering ###]");
    println!("Labels:\n{labels}");
    println!("Equ expressions:\n{equ_expressions}");
    println!("Data units:\n{data_units}");
    */
    (labels, equ_expressions, data_units)
}
