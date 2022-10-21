use strum::IntoEnumIterator;
use strum_macros::EnumIter;

use crate::prelude::*;

#[derive(Debug, PartialEq, EnumIter, Ord, PartialOrd, Eq, Copy, Clone)]
pub enum Register {
    Rax,
    Rcx,
    Rdx,
    Rbx,
    Rsp,
    Rbp,
    Rsi,
    Rdi,
    Rip,
}

impl Register {
    fn unsized_asm_name(&self) -> &'static str {
        match self {
            Rax => "ax",
            Rcx => "ax",
            Rdx => "dx",
            Rbx => "ax",
            Rsp => "sp",
            Rbp => "bp",
            Rsi => "si",
            Rdi => "di",
            Rip => "ip",
        }
    }

    pub fn asm_name(&self) -> String {
        format!("r{}", self.unsized_asm_name())
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub enum AccessType {
    // Low byte
    L,
    // High byte
    H,
    // 16-bit
    X,
    // 32-bit
    EX,
    // 64-bit
    RX,
}

#[derive(Debug, PartialEq)]
pub struct RegisterView(pub Register, pub AccessType);

impl RegisterView {
    pub fn asm_name(&self) -> String {
        let reg_name = self.0.unsized_asm_name();

        if [AccessType::L, AccessType::H].contains(&self.1) {
            // Most registers get their 'x' prefix trimmed
            // Registers with a different naming convention are passed through as-is
            let reg_prefix = match self.0 {
                Rsi | Rdi | Rsp | Rbp => reg_name.to_string(),
                _ => reg_name[0..reg_name.len() - 1].to_string(),
            };

            if self.1 == AccessType::L {
                format!("{reg_prefix}l")
            }
            else {
                // Certain registers don't offer a view of their high byte
                assert!(![Rsi, Rdi, Rsp, Rbp].contains(&self.0));
                format!("{reg_prefix}h")
            }
        }
        else {
            let prefix = match self.1 {
                AccessType::X => "",
                AccessType::EX => "e",
                AccessType::RX => "r",
                _ => panic!("Should never happen"),
            };
            format!("{prefix}{reg_name}")
        }
    }

    pub fn rax() -> Self {
        RegisterView(Rax, AccessType::RX)
    }

    pub fn eax() -> Self {
        RegisterView(Rax, AccessType::EX)
    }

    pub fn ax() -> Self {
        RegisterView(Rax, AccessType::X)
    }

    pub fn ah() -> Self {
        RegisterView(Rax, AccessType::H)
    }

    pub fn al() -> Self {
        RegisterView(Rax, AccessType::L)
    }

    pub fn rcx() -> Self {
        RegisterView(Rcx, AccessType::RX)
    }

    pub fn ecx() -> Self {
        RegisterView(Rcx, AccessType::EX)
    }

    pub fn cx() -> Self {
        RegisterView(Rcx, AccessType::X)
    }

    pub fn ch() -> Self {
        RegisterView(Rcx, AccessType::H)
    }

    pub fn cl() -> Self {
        RegisterView(Rcx, AccessType::L)
    }

    pub fn rdx() -> Self {
        RegisterView(Rdx, AccessType::RX)
    }

    pub fn edx() -> Self {
        RegisterView(Rdx, AccessType::EX)
    }

    pub fn dx() -> Self {
        RegisterView(Rdx, AccessType::X)
    }

    pub fn dh() -> Self {
        RegisterView(Rdx, AccessType::H)
    }

    pub fn dl() -> Self {
        RegisterView(Rdx, AccessType::L)
    }

    pub fn rbx() -> Self {
        RegisterView(Rbx, AccessType::RX)
    }

    pub fn ebx() -> Self {
        RegisterView(Rbx, AccessType::EX)
    }

    pub fn bx() -> Self {
        RegisterView(Rbx, AccessType::X)
    }

    pub fn bh() -> Self {
        RegisterView(Rbx, AccessType::H)
    }

    pub fn bl() -> Self {
        RegisterView(Rbx, AccessType::L)
    }

    pub fn rsp() -> Self {
        RegisterView(Rsp, AccessType::RX)
    }

    pub fn esp() -> Self {
        RegisterView(Rsp, AccessType::EX)
    }

    pub fn sp() -> Self {
        RegisterView(Rsp, AccessType::X)
    }

    pub fn spl() -> Self {
        RegisterView(Rsp, AccessType::L)
    }

    pub fn rbp() -> Self {
        RegisterView(Rbp, AccessType::RX)
    }

    pub fn ebp() -> Self {
        RegisterView(Rbp, AccessType::EX)
    }

    pub fn bp() -> Self {
        RegisterView(Rbp, AccessType::X)
    }

    pub fn bpl() -> Self {
        RegisterView(Rbp, AccessType::L)
    }

    pub fn rsi() -> Self {
        RegisterView(Rsi, AccessType::RX)
    }

    pub fn esi() -> Self {
        RegisterView(Rsi, AccessType::EX)
    }

    pub fn si() -> Self {
        RegisterView(Rsi, AccessType::X)
    }

    pub fn sil() -> Self {
        RegisterView(Rsi, AccessType::L)
    }

    pub fn rdi() -> Self {
        RegisterView(Rdi, AccessType::RX)
    }

    pub fn edi() -> Self {
        RegisterView(Rdi, AccessType::EX)
    }

    pub fn di() -> Self {
        RegisterView(Rdi, AccessType::X)
    }

    pub fn dil() -> Self {
        RegisterView(Rdi, AccessType::L)
    }
}
