use alloc::{borrow::ToOwned, string::String, vec::Vec};
use core::{cell::RefCell, fmt::Display};

use crate::assembly_packer::{next_atom_id, PotentialLabelTarget, PotentialLabelTargetId};
use crate::{assembly_parser::BinarySection, new_try::FileLayout};

#[derive(Debug, Copy, Clone)]
pub struct SymbolId(pub SymbolType, pub usize);
impl SymbolId {}

#[derive(Debug, Copy, Clone)]
pub enum SymbolType {
    _Code,
}

#[derive(Debug, PartialEq, Clone)]
pub enum SymbolExpressionOperand {
    OutputCursor,
    StartOfSymbol(String),
}

impl Display for SymbolExpressionOperand {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            SymbolExpressionOperand::OutputCursor => write!(f, "Op(.)"),
            SymbolExpressionOperand::StartOfSymbol(sym_name) => write!(f, "Op(SymStart(\"{}\"))", sym_name),
        }
    }
}

#[derive(Debug, PartialEq, Copy, Clone, Eq, PartialOrd, Ord)]
pub struct InstructionId(pub usize);

impl Display for InstructionId {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("InstrId({})", self.0))
    }
}

#[derive(Debug, PartialEq)]
pub enum SymbolData {
    LiteralData(Vec<u8>),
}

impl SymbolData {
    pub fn len(&self) -> usize {
        match self {
            SymbolData::LiteralData(data) => data.len(),
            // Takes up no size in .rodata as this symbol is represented only as a symbol table literal
        }
    }

    fn to_bytes(&self) -> Vec<u8> {
        match self {
            SymbolData::LiteralData(data) => data.to_owned(),
        }
    }
}

// TODO(PT): Delete DataSymbol in favor of ConstantData
#[derive(Debug, PartialEq)]
pub struct ConstantData {
    id: PotentialLabelTargetId,
    container_section: BinarySection,
    pub inner: SymbolData,
    pub immediate_value: RefCell<Option<u64>>,
}

impl ConstantData {
    pub fn new(container_section: BinarySection, inner: SymbolData) -> Self {
        Self {
            id: next_atom_id(),
            container_section,
            inner,
            immediate_value: RefCell::new(None),
        }
    }
}

impl Display for ConstantData {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("<ConstantData "))?;
        match &self.inner {
            SymbolData::LiteralData(data) => f.write_fmt(format_args!(" (literal, {} bytes)>", data.len())),
        }
    }
}

impl PotentialLabelTarget for ConstantData {
    fn container_section(&self) -> crate::assembly_parser::BinarySection {
        self.container_section
    }

    fn len(&self) -> usize {
        self.inner.len()
    }

    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        self.inner.to_bytes()
    }

    fn id(&self) -> crate::assembly_packer::PotentialLabelTargetId {
        self.id
    }
}
