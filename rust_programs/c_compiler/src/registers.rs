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
    Rflags
}

impl Register {
    fn unsized_asm_name(&self) -> &'static str {
        match self {
            Rax => "ax",
            Rcx => "cx",
            Rdx => "dx",
            Rbx => "bx",
            Rsp => "sp",
            Rbp => "bp",
            Rsi => "si",
            Rdi => "di",
            Rip => "ip",
            Rflags => "flags",
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

#[derive(Debug, PartialEq, Copy, Clone)]
pub struct RegView(pub Register, pub AccessType);

impl RegView {
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
        RegView(Rax, AccessType::RX)
    }

    pub fn eax() -> Self {
        RegView(Rax, AccessType::EX)
    }

    pub fn ax() -> Self {
        RegView(Rax, AccessType::X)
    }

    pub fn ah() -> Self {
        RegView(Rax, AccessType::H)
    }

    pub fn al() -> Self {
        RegView(Rax, AccessType::L)
    }

    pub fn rcx() -> Self {
        RegView(Rcx, AccessType::RX)
    }

    pub fn ecx() -> Self {
        RegView(Rcx, AccessType::EX)
    }

    pub fn cx() -> Self {
        RegView(Rcx, AccessType::X)
    }

    pub fn ch() -> Self {
        RegView(Rcx, AccessType::H)
    }

    pub fn cl() -> Self {
        RegView(Rcx, AccessType::L)
    }

    pub fn rdx() -> Self {
        RegView(Rdx, AccessType::RX)
    }

    pub fn edx() -> Self {
        RegView(Rdx, AccessType::EX)
    }

    pub fn dx() -> Self {
        RegView(Rdx, AccessType::X)
    }

    pub fn dh() -> Self {
        RegView(Rdx, AccessType::H)
    }

    pub fn dl() -> Self {
        RegView(Rdx, AccessType::L)
    }

    pub fn rbx() -> Self {
        RegView(Rbx, AccessType::RX)
    }

    pub fn ebx() -> Self {
        RegView(Rbx, AccessType::EX)
    }

    pub fn bx() -> Self {
        RegView(Rbx, AccessType::X)
    }

    pub fn bh() -> Self {
        RegView(Rbx, AccessType::H)
    }

    pub fn bl() -> Self {
        RegView(Rbx, AccessType::L)
    }

    pub fn rsp() -> Self {
        RegView(Rsp, AccessType::RX)
    }

    pub fn esp() -> Self {
        RegView(Rsp, AccessType::EX)
    }

    pub fn sp() -> Self {
        RegView(Rsp, AccessType::X)
    }

    pub fn spl() -> Self {
        RegView(Rsp, AccessType::L)
    }

    pub fn rbp() -> Self {
        RegView(Rbp, AccessType::RX)
    }

    pub fn ebp() -> Self {
        RegView(Rbp, AccessType::EX)
    }

    pub fn bp() -> Self {
        RegView(Rbp, AccessType::X)
    }

    pub fn bpl() -> Self {
        RegView(Rbp, AccessType::L)
    }

    pub fn rsi() -> Self {
        RegView(Rsi, AccessType::RX)
    }

    pub fn esi() -> Self {
        RegView(Rsi, AccessType::EX)
    }

    pub fn si() -> Self {
        RegView(Rsi, AccessType::X)
    }

    pub fn sil() -> Self {
        RegView(Rsi, AccessType::L)
    }

    pub fn rdi() -> Self {
        RegView(Rdi, AccessType::RX)
    }

    pub fn edi() -> Self {
        RegView(Rdi, AccessType::EX)
    }

    pub fn di() -> Self {
        RegView(Rdi, AccessType::X)
    }

    pub fn dil() -> Self {
        RegView(Rdi, AccessType::L)
    }

    pub fn rflags() -> Self {
        RegView(Rflags, AccessType::RX)
    }

    pub fn eflags() -> Self {
        RegView(Rflags, AccessType::EX)
    }
}
