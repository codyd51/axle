use crate::prelude::*;

pub enum RexPrefixOption {
    Use64BitOperandSize,
    _UseRegisterFieldExtension,
    _UseIndexFieldExtension,
    _UseBaseFieldExtension,
}

pub struct RexPrefix;
impl RexPrefix {
    fn from_options(options: Vec<RexPrefixOption>) -> u8 {
        let mut out = 0b0100 << 4;
        for option in options.iter() {
            match option {
                RexPrefixOption::Use64BitOperandSize => out |= 1 << 3,
                RexPrefixOption::_UseRegisterFieldExtension => out |= 1 << 2,
                RexPrefixOption::_UseIndexFieldExtension => out |= 1 << 1,
                RexPrefixOption::_UseBaseFieldExtension => out |= 1 << 0,
            }
        }
        out
    }

    pub fn for_64bit_operand() -> u8 {
        Self::from_options(vec![RexPrefixOption::Use64BitOperandSize])
    }
}

pub enum ModRmAddressingMode {
    RegisterDirect,
}

pub struct ModRmByte;
impl ModRmByte {
    pub fn register_index(register: Register) -> usize {
        match register {
            Rax => 0b000,
            Rcx => 0b001,
            Rdx => 0b010,
            Rbx => 0b011,
            Rsp => 0b100,
            Rbp => 0b101,
            Rsi => 0b110,
            Rdi => 0b111,
            _ => panic!("Invalid register for ModRm byte"),
        }
    }

    pub fn index_to_register(index: u8) -> Register {
        match index {
            0b000 => Rax,
            0b001 => Rcx,
            0b010 => Rdx,
            0b011 => Rbx,
            0b100 => Rsp,
            0b101 => Rbp,
            0b110 => Rsi,
            0b111 => Rdi,
            _ => panic!("Invalid register index for ModRm byte"),
        }
    }

    pub fn from(
        addressing_mode: ModRmAddressingMode,
        register: Register,
        register2: Option<Register>,
    ) -> u8 {
        let mut out = 0;

        match addressing_mode {
            ModRmAddressingMode::RegisterDirect => out |= 0b11 << 6,
        }

        out |= Self::register_index(register);

        if let Some(register2) = register2 {
            out |= Self::register_index(register2) << 3;
        }

        out as _
    }

    pub fn with_opcode_extension(
        addressing_mode: ModRmAddressingMode,
        opcode_extension: usize,
        register: RegView,
    ) -> u8 {
        assert!(
            opcode_extension <= 7,
            "opcode_extension must be in the range 0-7"
        );
        let mut out = 0;

        match addressing_mode {
            ModRmAddressingMode::RegisterDirect => out |= 0b11 << 6,
        }

        out |= opcode_extension << 3;
        out |= Self::register_index(register.0);

        out as _
    }

    pub fn get_opcode_extension(byte: u8) -> u8 {
        (byte >> 3) & 0b111
    }

    pub fn get_reg(byte: u8) -> Register {
        let reg_index = byte & 0b111;
        Self::index_to_register(reg_index)
    }

    pub fn get_regs(byte: u8) -> (Register, Register) {
        let reg1_index = byte & 0b111;
        let reg2_index = (byte >> 3) & 0b111;
        (
            Self::index_to_register(reg1_index),
            Self::index_to_register(reg2_index),
        )
    }
}
