extern crate derive_more;
use derive_more::Constructor;

use crate::prelude::*;

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveRegToReg {
    pub source: RegisterView,
    pub dest: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveImmToReg {
    pub imm: usize,
    pub dest: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveImmToRegMemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct AddRegToReg {
    pub augend: RegisterView,
    pub addend: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct SubRegFromReg {
    pub minuend: RegisterView,
    pub subtrahend: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MulRegByReg {
    pub multiplicand: RegisterView,
    pub multiplier: RegisterView,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct DivRegByReg {
    pub dividend: RegisterView,
    pub divisor: RegisterView,
}

#[derive(Debug, PartialEq)]
pub enum Instr {
    Return,
    PushFromReg(RegisterView),
    PopIntoReg(RegisterView),
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
