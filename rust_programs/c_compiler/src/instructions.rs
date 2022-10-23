extern crate derive_more;
use derive_more::Constructor;

use crate::prelude::*;

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct MoveRegToReg {
    pub source: RegView,
    pub dest: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct MoveImmToReg {
    pub imm: usize,
    pub dest: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct MoveImmToRegMemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct AddRegToReg {
    pub augend: RegView,
    pub addend: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct SubRegFromReg {
    pub minuend: RegView,
    pub subtrahend: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct MulRegByReg {
    pub multiplicand: RegView,
    pub multiplier: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct DivRegByReg {
    pub dividend: RegView,
    pub divisor: RegView,
}

#[derive(Debug, PartialEq, Clone, Constructor)]
pub struct CompareImmWithReg {
    pub imm: usize,
    pub reg: RegView,
}

#[derive(Debug, PartialEq, Clone)]
pub enum Instr {
    Return,
    PushFromReg(RegView),
    PopIntoReg(RegView),
    MoveRegToReg(MoveRegToReg),
    MoveImmToReg(MoveImmToReg),
    DirectiveDeclareGlobalSymbol(String),
    DirectiveDeclareLabel(String),
    MoveImm8oRegMemOffset(MoveImmToRegMemOffset),
    NegateRegister(Register),
    AddRegToReg(AddRegToReg),
    SubRegFromReg(SubRegFromReg),
    MulRegByReg(MulRegByReg),
    DivRegByReg(DivRegByReg),
    CompareImmWithReg(CompareImmWithReg),
}

impl Instr {
    pub fn render(&self) -> String {
        match self {
            Instr::Return => "ret".into(),
            Instr::PushFromReg(src) => format!("push %{}", src.asm_name()),
            Instr::PopIntoReg(dst) => format!("pop %{}", dst.asm_name()),
            Instr::MoveRegToReg(MoveRegToReg { source, dest }) => {
                format!("mov %{}, %{}", source.asm_name(), dest.asm_name())
            }
            Instr::MoveImmToReg(MoveImmToReg { imm, dest }) => {
                format!("mov ${imm}, %{}", dest.asm_name())
            }
            Instr::DirectiveDeclareGlobalSymbol(symbol_name) => {
                format!(".global {symbol_name}")
            }
            Instr::DirectiveDeclareLabel(label_name) => format!("{label_name}:"),
            Instr::NegateRegister(reg) => format!("neg %{}", reg.asm_name()),
            Instr::AddRegToReg(AddRegToReg { augend, addend }) => {
                format!("add %{}, %{}", augend.asm_name(), addend.asm_name())
            }
            _ => todo!(),
        }
    }
}
