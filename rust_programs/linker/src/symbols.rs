use alloc::{
    borrow::ToOwned,
    rc::Rc,
    string::{String, ToString},
    vec::Vec,
};
use core::{cell::RefCell, fmt::Display};

use crate::println;
use crate::{
    assembly_packer::Instruction,
    new_try::{FileLayout, RebaseTarget, RebasedValue},
};

#[derive(Debug, Copy, Clone)]
pub struct SymbolId(pub SymbolType, pub usize);
impl SymbolId {
    pub fn null_id() -> Self {
        Self(SymbolType::Null, 0)
    }
}

#[derive(Debug, Copy, Clone)]
pub enum SymbolType {
    Null,
    Data,
    _Code,
}

#[derive(Debug, PartialEq)]
pub enum SymbolExpressionOperand {
    OutputCursor,
    StartOfSymbol(String),
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
    Subtract((SymbolExpressionOperand, SymbolExpressionOperand)),
    InstructionAddress(InstructionId),
}

impl SymbolData {
    pub fn len(&self) -> usize {
        match self {
            SymbolData::LiteralData(data) => data.len(),
            // Takes up no size in .rodata as this symbol is represented only as a symbol table literal
            SymbolData::Subtract((op1, op2)) => 0,
            // Takes up no size in .rodata
            SymbolData::InstructionAddress(_) => 0,
        }
    }

    fn to_bytes(&self) -> Vec<u8> {
        match self {
            SymbolData::LiteralData(data) => data.to_owned(),
            SymbolData::Subtract(_) => panic!("Unexpected"),
            SymbolData::InstructionAddress(_) => panic!("Unexpected"),
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct DataSymbol {
    pub name: String,
    pub inner: SymbolData,
    pub immediate_value: RefCell<Option<u64>>,
}

impl DataSymbol {
    pub fn new(name: &str, inner: SymbolData) -> Self {
        Self {
            name: name.to_string(),
            inner,
            immediate_value: RefCell::new(None),
        }
    }

    fn set_immediate_value(&self, value: u64) {
        let mut immediate_value = self.immediate_value.borrow_mut();
        *immediate_value = Some(value);
    }

    pub fn render(&self, layout: &FileLayout, current_data_cursor: usize) -> Option<Vec<u8>> {
        match &self.inner {
            SymbolData::LiteralData(data) => {
                let start_addr = layout.get_rebased_value(RebasedValue::VirtStartOf(RebaseTarget::ReadOnlyDataSection)) + current_data_cursor;
                self.set_immediate_value(start_addr as _);
                Some(data.to_owned())
            }
            SymbolData::Subtract((op1, op2)) => {
                let op1_value = match op1 {
                    SymbolExpressionOperand::OutputCursor => {
                        layout.get_rebased_value(RebasedValue::VirtStartOf(RebaseTarget::ReadOnlyDataSection)) + current_data_cursor
                    }
                    _ => todo!(),
                };
                let op2_value = match op2 {
                    SymbolExpressionOperand::OutputCursor => todo!(),
                    SymbolExpressionOperand::StartOfSymbol(other_sym) => {
                        let symbol_id = layout.get_symbol_id_for_symbol_name(other_sym);
                        layout.get_rebased_value(RebasedValue::ReadOnlyDataSymbolVirtAddr(symbol_id))
                    }
                };
                let value = (op1_value - op2_value) as u64;
                self.set_immediate_value(value);
                println!("Computing .equ expression: {} = {op1:?} - {op2:?}", self.name);
                println!("                           {} = {op1_value:016x} - {op2_value:016x}", self.name);
                println!("                           {} = {value:016x}", self.name);

                None
            }
            SymbolData::InstructionAddress(instruction_id) => {
                let instruction_address = layout.get_rebased_value(RebasedValue::InstructionAddress(*instruction_id));
                self.set_immediate_value(instruction_address as _);
                None
            }
        }
    }
}

impl Display for DataSymbol {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("<DataSymbol \"{}\"", self.name))?;
        match &self.inner {
            SymbolData::LiteralData(data) => f.write_fmt(format_args!(" (literal, {} bytes)>", data.len())),
            SymbolData::Subtract((op1, op2)) => f.write_fmt(format_args!(" (subtraction, {:?} - {:?})>", op1, op2)),
            SymbolData::InstructionAddress(instruction_id) => f.write_fmt(format_args!(" (instruction #{instruction_id}")),
        }
    }
}
