use crate::prelude::*;

#[derive(Debug, PartialEq)]
pub struct MoveReg8ToReg8 {
    pub source: Register,
    pub dest: Register,
}

impl MoveReg8ToReg8 {
    pub fn new(source: Register, dest: Register) -> Self {
        Self { source, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm8ToReg8 {
    pub imm: usize,
    pub dest: Register,
}

impl MoveImm8ToReg8 {
    pub fn new(imm: usize, dest: Register) -> Self {
        Self { imm, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm32ToReg32 {
    pub imm: usize,
    pub dest: Register,
}

impl MoveImm32ToReg32 {
    pub fn new(imm: usize, dest: Register) -> Self {
        Self { imm, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm8ToReg8MemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: Register,
}

impl MoveImm8ToReg8MemOffset {
    pub fn new(imm: usize, offset: isize, reg_to_deref: Register) -> Self {
        Self {
            imm,
            offset,
            reg_to_deref,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct AddReg32ToReg32 {
    pub augend: Register,
    pub addend: Register,
}

impl AddReg32ToReg32 {
    pub fn new(augend: Register, addend: Register) -> Self {
        Self {
            augend,
            addend,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct SubReg32FromReg32 {
    pub minuend: Register,
    pub subtrahend: Register,
}

impl SubReg32FromReg32 {
    pub fn new(minuend: Register, subtrahend: Register) -> Self {
        Self {
            minuend,
            subtrahend,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct MulReg32ByReg32 {
    pub multiplicand: Register,
    pub multiplier: Register,
}

impl MulReg32ByReg32 {
    pub fn new(multiplicand: Register, multiplier: Register) -> Self {
        Self {
            multiplicand,
            multiplier,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct DivReg32ByReg32 {
    pub dividend: Register,
    pub divisor: Register,
}

impl DivReg32ByReg32 {
    pub fn new(dividend: Register, divisor: Register) -> Self {
        Self {
            dividend,
            divisor,
        }
    }
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
