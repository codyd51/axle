extern crate derive_more;
use derive_more::Constructor;

use crate::prelude::*;

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveReg8ToReg8 {
    pub source: Register,
    pub dest: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveImm8ToReg8 {
    pub imm: usize,
    pub dest: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveImm32ToReg32 {
    pub imm: usize,
    pub dest: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MoveImm8ToReg8MemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct AddReg32ToReg32 {
    pub augend: Register,
    pub addend: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct SubReg32FromReg32 {
    pub minuend: Register,
    pub subtrahend: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct MulReg32ByReg32 {
    pub multiplicand: Register,
    pub multiplier: Register,
}

#[derive(Debug, PartialEq, Constructor)]
pub struct DivReg32ByReg32 {
    pub dividend: Register,
    pub divisor: Register,
}

#[derive(Debug, PartialEq)]
pub enum Instr {
    Return,
    PushFromReg(RegisterView),
    PopIntoReg(RegisterView),
    MoveReg8ToReg8(MoveReg8ToReg8),
    MoveImm8ToReg8(MoveImm8ToReg8),
    MoveImm32ToReg32(MoveImm32ToReg32),
    DirectiveDeclareGlobalSymbol(String),
    DirectiveDeclareLabel(String),
    MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset),
    NegateRegister(Register),
    AddReg8ToReg8(AddReg32ToReg32),
    SubReg32FromReg32(SubReg32FromReg32),
    MulReg32ByReg32(MulReg32ByReg32),
    DivReg32ByReg32(DivReg32ByReg32),
}

impl Instr {
    pub fn render(&self) -> String {
        match self {
            Instr::Return => "ret".into(),
            Instr::PushFromReg(src) => format!("push %{}", src.asm_name()),
            Instr::PopIntoReg(dst) => format!("pop %{}", dst.asm_name()),
            Instr::MoveReg8ToReg8(MoveReg8ToReg8 { source, dest }) => {
                format!("mov %{}, %{}", source.asm_name(), dest.asm_name())
            }
            Instr::MoveImm8ToReg8(MoveImm8ToReg8 { imm, dest }) => {
                format!("mov ${imm}, %{}", dest.asm_name())
            }
            Instr::DirectiveDeclareGlobalSymbol(symbol_name) => {
                format!(".global {symbol_name}")
            }
            Instr::DirectiveDeclareLabel(label_name) => format!("{label_name}:"),
            Instr::NegateRegister(reg) => format!("neg %{}", reg.asm_name()),
            Instr::AddReg8ToReg8(AddReg32ToReg32 { augend, addend }) => {
                format!("add %{}, %{}", augend.asm_name(), addend.asm_name())
            }
            _ => todo!(),
        }
    }
}
