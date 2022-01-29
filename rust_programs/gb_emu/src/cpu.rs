use core::mem;
use std::{
    cell::RefCell,
    collections::BTreeMap,
    env::VarError,
    fmt::{Debug, Display},
    rc::Rc,
};

use alloc::vec::Vec;

use bitmatch::bitmatch;

use crate::mmu::Mmu;

pub struct InstrInfo {
    pub instruction_size: u16,
    pub cycle_count: usize,
    pc_increment: Option<u16>,
    jumped: bool,
}

impl InstrInfo {
    fn seq(instruction_size: u16, cycle_count: usize) -> Self {
        InstrInfo {
            instruction_size,
            cycle_count,
            pc_increment: Some(instruction_size),
            jumped: false,
        }
    }
    fn jump(instruction_size: u16, cycle_count: usize) -> Self {
        InstrInfo {
            instruction_size,
            cycle_count,
            // Don't increment pc because the instruction will modify pc directly
            pc_increment: None,
            jumped: true,
        }
    }
}

#[derive(Debug, PartialEq)]
enum Flag {
    Zero,
    Subtract,
    Carry,
    HalfCarry,
}

enum FlagUpdate {
    Zero(bool),
    Subtract(bool),
    Carry(bool),
    HalfCarry(bool),
}

#[derive(Copy, Clone)]
enum FlagCondition {
    NotZero,
    Zero,
    NotCarry,
    Carry,
}

impl Display for FlagCondition {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let name = match self {
            FlagCondition::NotZero => "NZ",
            FlagCondition::Zero => "Z",
            FlagCondition::NotCarry => "NC",
            FlagCondition::Carry => "C",
        };
        write!(f, "{}", name)
    }
}

pub struct CpuState {
    operands: BTreeMap<RegisterName, Box<dyn VariableStorage>>,
    mmu: Rc<Mmu>,
    debug_enabled: bool,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum RegisterName {
    // 8-bit operands
    B,
    C,
    D,
    E,
    H,
    L,
    A,
    F,

    // 16-bit operands
    BC,
    DE,
    HL,
    AF,

    SP,
    PC,
}

impl Display for RegisterName {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let name = match self {
            RegisterName::B => "B",
            RegisterName::C => "C",
            RegisterName::D => "D",
            RegisterName::E => "E",
            RegisterName::H => "H",
            RegisterName::L => "L",
            RegisterName::A => "A",
            RegisterName::F => "F",

            RegisterName::BC => "BC",
            RegisterName::DE => "DE",
            RegisterName::HL => "HL",
            RegisterName::AF => "AF",

            RegisterName::SP => "SP",
            RegisterName::PC => "PC",
        };
        write!(f, "{}", name)
    }
}

trait VariableStorage: Debug + Display {
    fn display_name(&self) -> &str;

    fn read_u8(&self, cpu: &CpuState) -> u8;
    fn read_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode) -> u8;
    fn read_u16(&self, _cpu: &CpuState) -> u16;

    fn write_u8(&self, cpu: &CpuState, val: u8);
    fn write_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode, val: u8);
    fn write_u16(&self, cpu: &CpuState, val: u16);
}

#[derive(Debug)]
struct CpuRegister {
    name: String,
    contents: RefCell<u8>,
}

impl CpuRegister {
    fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            contents: RefCell::new(0),
        }
    }
}

impl VariableStorage for CpuRegister {
    fn display_name(&self) -> &str {
        &self.name
    }

    fn read_u8(&self, cpu: &CpuState) -> u8 {
        self.read_u8_with_mode(cpu, AddressingMode::Read)
    }

    fn read_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode) -> u8 {
        match addressing_mode {
            AddressingMode::Read => *self.contents.borrow(),
            other => panic!("Addressing mode not available for CpuRegister: {other}"),
        }
    }

    fn read_u16(&self, _cpu: &CpuState) -> u16 {
        panic!("Cannot read u16 from 8bit register")
    }

    fn write_u8(&self, cpu: &CpuState, val: u8) {
        self.write_u8_with_mode(cpu, AddressingMode::Read, val)
    }

    fn write_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode, val: u8) {
        match addressing_mode {
            AddressingMode::Read => *self.contents.borrow_mut() = val,
            other => panic!("Addressing mode not available for CpuRegister: {other}"),
        }
    }

    fn write_u16(&self, _cpu: &CpuState, val: u16) {
        panic!("Cannot write u16 to 8bit register")
    }
}

impl Display for CpuRegister {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}({:02x})", self.name, *self.contents.borrow())
    }
}

#[derive(Debug)]
struct CpuRegisterPair {
    upper: RegisterName,
    lower: RegisterName,
}

impl CpuRegisterPair {
    fn new(upper: RegisterName, lower: RegisterName) -> Self {
        Self { upper, lower }
    }
}

impl VariableStorage for CpuRegisterPair {
    fn display_name(&self) -> &str {
        "RegPair"
    }

    fn read_u8(&self, cpu: &CpuState) -> u8 {
        // Implicitly dereference the address pointed to by the register pair
        self.read_u8_with_mode(cpu, AddressingMode::Deref)
    }

    fn read_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode) -> u8 {
        let address = self.read_u16(cpu);
        match addressing_mode {
            AddressingMode::Read => panic!("Register pair cannot directly be read as u8"),
            AddressingMode::Deref => cpu.mmu.read(address),
            AddressingMode::DerefThenIncrement => {
                self.write_u16(cpu, address + 1);
                cpu.mmu.read(address)
            }
            AddressingMode::DerefThenDecrement => {
                self.write_u16(cpu, address - 1);
                cpu.mmu.read(address)
            }
        }
    }

    fn read_u16(&self, cpu: &CpuState) -> u16 {
        let upper = cpu.reg(self.upper);
        let lower = cpu.reg(self.lower);
        ((upper.read_u8(cpu) as u16) << 8) | (lower.read_u8(cpu) as u16)
    }

    fn write_u8(&self, cpu: &CpuState, val: u8) {
        // Implicitly dereference the memory pointed to by the register pair
        self.write_u8_with_mode(cpu, AddressingMode::Deref, val)
    }

    fn write_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode, val: u8) {
        let address = self.read_u16(cpu);
        match addressing_mode {
            AddressingMode::Read => {
                panic!("'Read' is not a valid addressing mode when writing a u8")
            }
            AddressingMode::Deref => cpu.mmu.write(address, val),
            AddressingMode::DerefThenIncrement => {
                self.write_u16(cpu, address + 1);
                cpu.mmu.write(address, val)
            }
            AddressingMode::DerefThenDecrement => {
                self.write_u16(cpu, address - 1);
                cpu.mmu.write(address, val)
            }
        }
    }

    fn write_u16(&self, cpu: &CpuState, val: u16) {
        let upper = cpu.reg(self.upper);
        let lower = cpu.reg(self.lower);

        upper.write_u8(cpu, ((val >> 8) & 0xff).try_into().unwrap());
        lower.write_u8(cpu, (val & 0xff).try_into().unwrap());
    }
}

impl Display for CpuRegisterPair {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{}{}", self.upper, self.lower)
    }
}

#[derive(Debug)]
struct CpuWideRegister {
    display_name: String,
    name: RegisterName,
    contents: RefCell<u16>,
}

impl CpuWideRegister {
    fn new(name: RegisterName) -> Self {
        Self {
            display_name: format!("{}", name),
            name,
            contents: RefCell::new(0),
        }
    }
}

impl VariableStorage for CpuWideRegister {
    fn display_name(&self) -> &str {
        &self.display_name
    }

    fn read_u8(&self, cpu: &CpuState) -> u8 {
        // TODO(PT): Move this error into the read_u8_with_mode() impl
        panic!("Wide register cannot read u8")
    }

    fn read_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode) -> u8 {
        todo!()
    }

    fn read_u16(&self, cpu: &CpuState) -> u16 {
        (*self.contents.borrow()).into()
    }

    fn write_u8(&self, _cpu: &CpuState, val: u8) {
        panic!("Wide register cannot write u8")
    }

    fn write_u8_with_mode(&self, cpu: &CpuState, addressing_mode: AddressingMode, val: u8) {
        todo!()
    }

    fn write_u16(&self, cpu: &CpuState, val: u16) {
        *self.contents.borrow_mut() = val
    }
}

impl Display for CpuWideRegister {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "[{} {:04x}]", self.display_name, *self.contents.borrow())
    }
}

#[derive(Debug, Copy, Clone)]
enum AddressingMode {
    Read,
    Deref,
    DerefThenIncrement,
    DerefThenDecrement,
}

impl Display for AddressingMode {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let name = match self {
            AddressingMode::Read => "",
            AddressingMode::Deref => "[]",
            AddressingMode::DerefThenIncrement => "[+]",
            AddressingMode::DerefThenDecrement => "[-]",
        };
        write!(f, "{}", name)
    }
}

// OpInfo { OpName, Op, DerefMode }

impl CpuState {
    pub fn new(mmu: Rc<Mmu>) -> Self {
        let mut operands: BTreeMap<RegisterName, Box<dyn VariableStorage>> = BTreeMap::new();

        // 8-bit operands
        operands.insert(RegisterName::B, Box::new(CpuRegister::new("B")));
        operands.insert(RegisterName::C, Box::new(CpuRegister::new("C")));
        operands.insert(RegisterName::D, Box::new(CpuRegister::new("D")));
        operands.insert(RegisterName::E, Box::new(CpuRegister::new("E")));
        operands.insert(RegisterName::H, Box::new(CpuRegister::new("H")));
        operands.insert(RegisterName::L, Box::new(CpuRegister::new("L")));
        operands.insert(RegisterName::A, Box::new(CpuRegister::new("A")));
        operands.insert(RegisterName::F, Box::new(CpuRegister::new("F")));

        // 16-bit operands
        operands.insert(
            RegisterName::BC,
            Box::new(CpuRegisterPair::new(RegisterName::B, RegisterName::C)),
        );
        operands.insert(
            RegisterName::DE,
            Box::new(CpuRegisterPair::new(RegisterName::D, RegisterName::E)),
        );
        operands.insert(
            RegisterName::HL,
            Box::new(CpuRegisterPair::new(RegisterName::H, RegisterName::L)),
        );
        operands.insert(
            RegisterName::AF,
            Box::new(CpuRegisterPair::new(RegisterName::A, RegisterName::F)),
        );
        // Stack pointer
        operands.insert(
            RegisterName::SP,
            Box::new(CpuWideRegister::new(RegisterName::SP)),
        );
        // Program counter
        operands.insert(
            RegisterName::PC,
            Box::new(CpuWideRegister::new(RegisterName::PC)),
        );

        Self {
            operands,
            mmu,
            debug_enabled: false,
        }
    }

    pub fn get_pc(&self) -> u16 {
        // TODO(PT): Rename Operand to Register?
        self.reg(RegisterName::PC).read_u16(self)
    }

    pub fn set_pc(&self, val: u16) {
        self.reg(RegisterName::PC).write_u16(self, val)
    }

    pub fn enable_debug(&mut self) {
        self.debug_enabled = true;
    }

    pub fn print_regs(&self) {
        println!();
        println!("--- CPU State ---");
        let flags = self.format_flags();
        println!(
            "\t{}\t{}",
            self.reg(RegisterName::PC),
            self.reg(RegisterName::SP)
        );
        println!("\tFlags: {flags}");

        for (name, operand) in &self.operands {
            // Some registers are handled above
            if ![RegisterName::PC, RegisterName::SP].contains(name) {
                println!("\t{name}: {operand}");
            }
        }
    }

    fn set_flags(&mut self, z: bool, n: bool, h: bool, c: bool) {
        let high_nibble = c as u8 | ((h as u8) << 1) | ((n as u8) << 2) | ((z as u8) << 3);
        self.reg(RegisterName::F).write_u8(self, high_nibble << 4);
    }

    fn update_flag(&self, flag: FlagUpdate) {
        let mut flag_setting_and_bit_index = match flag {
            FlagUpdate::Zero(on) => (on, 3),
            FlagUpdate::Subtract(on) => (on, 2),
            FlagUpdate::HalfCarry(on) => (on, 1),
            FlagUpdate::Carry(on) => (on, 0),
        };
        // Flags are always in the high nibble
        let bit_index = 4 + flag_setting_and_bit_index.1;

        let flags_reg = self.reg(RegisterName::F);
        let mut flags = flags_reg.read_u8(self);
        if flag_setting_and_bit_index.0 {
            // Enable flag
            flags |= 1 << bit_index;
        } else {
            // Disable flag
            flags &= !(1 << bit_index);
        }
        flags_reg.write_u8(self, flags);
    }

    fn is_flag_set(&self, flag: Flag) -> bool {
        let mut flag_bit_index = match flag {
            Flag::Zero => 3,
            Flag::Subtract => 2,
            Flag::HalfCarry => 1,
            Flag::Carry => 0,
        };
        let flags = self.reg(RegisterName::F).read_u8(self);
        (flags & (1 << (4 + flag_bit_index))) != 0
    }

    fn is_flag_condition_met(&self, cond: FlagCondition) -> bool {
        match cond {
            FlagCondition::NotZero => !self.is_flag_set(Flag::Zero),
            FlagCondition::Zero => self.is_flag_set(Flag::Zero),
            FlagCondition::NotCarry => !self.is_flag_set(Flag::Carry),
            FlagCondition::Carry => self.is_flag_set(Flag::Carry),
        }
    }

    pub fn format_flags(&self) -> String {
        format!(
            "{}{}{}{}",
            if self.is_flag_set(Flag::Carry) {
                "C"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::HalfCarry) {
                "H"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::Subtract) {
                "N"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::Zero) {
                "Z"
            } else {
                "-"
            },
        )
    }

    fn get_reg_name_and_addressing_mode_from_lookup_tab1(
        &self,
        index: u8,
    ) -> (RegisterName, AddressingMode) {
        match index {
            0 => (RegisterName::B, AddressingMode::Read),
            1 => (RegisterName::C, AddressingMode::Read),
            2 => (RegisterName::D, AddressingMode::Read),
            3 => (RegisterName::E, AddressingMode::Read),
            4 => (RegisterName::H, AddressingMode::Read),
            5 => (RegisterName::L, AddressingMode::Read),
            7 => (RegisterName::A, AddressingMode::Read),

            6 => (RegisterName::HL, AddressingMode::Deref),

            _ => panic!("Invalid index"),
        }
    }

    fn get_reg_name_and_addressing_mode_from_lookup_tab2(
        &self,
        index: u8,
    ) -> (RegisterName, AddressingMode) {
        match index {
            0 => (RegisterName::BC, AddressingMode::Read),
            1 => (RegisterName::DE, AddressingMode::Read),
            2 => (RegisterName::HL, AddressingMode::Read),
            3 => (RegisterName::SP, AddressingMode::Read),
            _ => panic!("Invalid index"),
        }
    }

    fn get_reg_name_and_addressing_mode_from_lookup_tab3(
        &self,
        index: u8,
    ) -> (RegisterName, AddressingMode) {
        match index {
            0 => (RegisterName::BC, AddressingMode::Deref),
            1 => (RegisterName::DE, AddressingMode::Deref),
            2 => (RegisterName::HL, AddressingMode::DerefThenIncrement),
            3 => (RegisterName::HL, AddressingMode::DerefThenDecrement),
            _ => panic!("Invalid index"),
        }
    }

    fn get_reg_from_lookup_tab1(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_reg_name_and_addressing_mode_from_lookup_tab1(index);
        (self.reg(name), mode)
    }

    fn get_reg_from_lookup_tab2(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_reg_name_and_addressing_mode_from_lookup_tab2(index);
        (self.reg(name), mode)
    }

    fn get_reg_from_lookup_tab3(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_reg_name_and_addressing_mode_from_lookup_tab3(index);
        (self.reg(name), mode)
    }

    pub fn reg(&self, name: RegisterName) -> &dyn VariableStorage {
        &*self.operands[&name]
    }

    fn rlc_or_rl(
        &self,
        reg: &dyn VariableStorage,
        addressing_mode: AddressingMode,
        is_rlc: bool,
    ) -> InstrInfo {
        // Extracted because this is used in both the normal and CB tables
        if self.debug_enabled {
            if is_rlc {
                println!("RLC {reg}");
            } else {
                println!("RL {reg}");
            }
        }

        let contents = reg.read_u8_with_mode(&self, addressing_mode);
        let high_bit = contents >> 7;

        // Left shift
        let mut rotated = (contents << 1);
        if is_rlc {
            // Copy the high bit back into bit 0
            rotated |= high_bit;
        } else {
            // Shift and set the LSB to the contents of the C flag
            let carry = match self.is_flag_set(Flag::Carry) {
                true => 1,
                false => 0,
            };
            rotated |= carry;
        }
        reg.write_u8_with_mode(self, addressing_mode, rotated);
        // Copy the high bit into the C flag
        self.update_flag(FlagUpdate::Carry(high_bit == 1));
        self.update_flag(FlagUpdate::Zero(rotated == 0));
        self.update_flag(FlagUpdate::HalfCarry(false));
        self.update_flag(FlagUpdate::Subtract(false));
        // TODO(PT): Should be 4 cycles when (HL)
        InstrInfo::seq(2, 2)
    }

    fn instr_cp(&self, val: u8) -> InstrInfo {
        let a = self.reg(RegisterName::A);
        let a_val = a.read_u8(&self);
        let (result, did_overflow) = a_val.overflowing_sub(val);
        self.update_flag(FlagUpdate::Zero(a_val == val));
        self.update_flag(FlagUpdate::Subtract(true));
        // Underflow into the high nibble?
        self.update_flag(FlagUpdate::HalfCarry((a_val & 0xf) < (result & 0xf)));
        // Underflow into the next byte?
        self.update_flag(FlagUpdate::Carry(did_overflow));
        InstrInfo::seq(2, 2)
    }

    #[bitmatch]
    fn decode_cb_prefixed_instr(&mut self, instruction_byte: u8) -> usize {
        let debug = self.debug_enabled;
        if debug {
            print!("0x{:04x}\tcb {:02x}\t\t", self.get_pc(), instruction_byte);
        }

        // Classes of instructions are handled as a group
        // PT: Manually derived these groups by inspecting the opcode table
        // Opcode table ref: https://meganesulli.com/generate-gb-opcodes/
        #[bitmatch]
        match instruction_byte {
            "01bbbiii" => {
                // BIT B, Reg8
                let bit_to_test = b;
                let (reg, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("BIT {bit_to_test}, {reg}");
                }
                let contents = reg.read_u8_with_mode(&self, read_mode);
                let bit_not_set = contents & (1 << bit_to_test) == 0;
                self.update_flag(FlagUpdate::Zero(bit_not_set));
                self.update_flag(FlagUpdate::Subtract(false));
                self.update_flag(FlagUpdate::HalfCarry(true));
                // TODO(PT): Should be three cycles when (HL)
                2
            }
            "000c0iii" => {
                // RLC Reg8 | RL Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                let is_rlc = c == 0;
                self.rlc_or_rl(reg, addressing_mode, is_rlc).cycle_count
            }
            _ => {
                println!("<cb {:02x} is unimplemented>", instruction_byte);
                self.print_regs();
                panic!("Unimplemented CB opcode")
            }
        }
    }

    #[bitmatch]
    fn decode(&mut self, pc: u16) -> InstrInfo {
        // Fetch the next opcode
        let instruction_byte: u8 = self.mmu.read(pc);

        // Handle CB-prefixed instructions
        if instruction_byte == 0xcb {
            let instruction_byte2: u8 = self.mmu.read(pc + 1);
            let cycle_count = self.decode_cb_prefixed_instr(instruction_byte2);
            return InstrInfo::seq(2, cycle_count);
        }

        // Decode using the strategy described by https://gb-archive.github.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html
        let opcode_digit1 = (instruction_byte >> 6) & 0b11;
        let opcode_digit2 = (instruction_byte >> 3) & 0b111;
        let opcode_digit3 = (instruction_byte >> 0) & 0b111;

        let debug = self.debug_enabled;
        if debug {
            print!("0x{:04x}\t{:02x}\t\t", self.get_pc(), instruction_byte);
        }

        // Some instructions have dedicated handling
        let maybe_instr_info = match instruction_byte {
            0x00 => {
                if debug {
                    println!("NOP");
                }
                Some(InstrInfo::seq(1, 1))
            }
            0x18 => {
                let rel_target: i8 = self.mmu.read(self.get_pc() + 1) as i8;
                if debug {
                    println!("JR +{:02x};\t", rel_target);
                }
                // Add 2 to PC before doing the relative target, as
                // this instruction is 2 bytes wide
                let mut pc = self.get_pc();
                pc += 2;
                pc = ((pc as i16) + rel_target as i16) as u16;
                self.set_pc(pc);
                Some(InstrInfo::jump(2, 3))
            }
            0x76 => {
                todo!("HALT")
            }
            0xc3 => {
                // JP u16
                let target = self.mmu.read_u16(self.get_pc() + 1);
                if debug {
                    println!("JMP 0x{target:04x}");
                }
                self.set_pc(target);
                Some(InstrInfo::jump(3, 4))
            }
            0xcd => {
                // CALL u16
                let current_pc = self.get_pc();
                let target = self.mmu.read_u16(current_pc + 1);
                if debug {
                    println!("CALL 0x{target:04x}");
                }
                // Store the return address on the stack
                // After the call completes,
                // return to the address after 1-byte opcode and 2-byte jump target
                let instr_size = 3;
                self.push_u16(current_pc + instr_size);
                // Assign PC to the jump target
                self.set_pc(target);
                Some(InstrInfo::jump(instr_size, 6))
            }
            0xc9 => {
                // RET
                self.set_pc(self.pop_u16());
                if debug {
                    println!("RET {:04x}", self.get_pc());
                }
                Some(InstrInfo::jump(1, 4))
            }
            0xfe => {
                // CP u8
                let val = self.mmu.read(self.get_pc() + 1);
                let a = self.reg(RegisterName::A);
                if debug {
                    println!("CP {val:02x} with {a}");
                }
                self.instr_cp(val);
                Some(InstrInfo::seq(2, 2))
            }
            0xbe => {
                // CP (HL)
                let val = self
                    .reg(RegisterName::HL)
                    .read_u8_with_mode(&self, AddressingMode::Deref);
                let a = self.reg(RegisterName::A);

                if debug {
                    println!("CP {}{val:02x} with {a}", self.reg(RegisterName::HL));
                }

                self.instr_cp(val);
                Some(InstrInfo::seq(1, 2))
            }
            // Handled down below
            _ => None,
        };
        if let Some(instr_info) = maybe_instr_info {
            // Instruction interpreted by direct opcode match
            return instr_info;
        }

        // Classes of instructions are handled as a group
        // PT: Manually derived these groups by inspecting the opcode table
        // Opcode table ref: https://meganesulli.com/generate-gb-opcodes/
        #[bitmatch]
        match instruction_byte {
            "00iii100" => {
                // INC Reg8
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("INC {op}");
                }
                let prev = op.read_u8_with_mode(&self, read_mode);
                let increment = 1;
                let new = prev.overflowing_add(increment).0;
                op.write_u8(&self, new);
                self.update_flag(FlagUpdate::Zero(new == 0));
                self.update_flag(FlagUpdate::Subtract(false));
                let half_carry_flag =
                    ((((prev as u16) & 0xf) + ((increment as u16) & 0xf)) & 0x10) == 0x10;
                self.update_flag(FlagUpdate::HalfCarry(half_carry_flag));
                // TODO(PT): Cycle count should be 3 for (HL)
                InstrInfo::seq(1, 1)
            }
            "00iii101" => {
                // DEC [Reg]
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    print!("DEC {op}\t");
                }
                let prev = op.read_u8_with_mode(&self, read_mode);
                let new = prev.overflowing_sub(1).0;
                op.write_u8(&self, new);
                self.update_flag(FlagUpdate::Zero(new == 0));
                self.update_flag(FlagUpdate::Subtract(true));
                // Underflow into the high nibble?
                self.update_flag(FlagUpdate::HalfCarry((prev & 0xf) < (new & 0xf)));

                if debug {
                    println!("Result: {op}")
                }
                // TODO(PT): Should be 3 for (HL)
                InstrInfo::seq(1, 1)
            }
            "01tttfff" => {
                // Opcode is 0x40 to 0x7f
                // LD [Reg], [Reg]
                let (from_op, from_op_read_mode) = self.get_reg_from_lookup_tab1(f);
                let from_val = from_op.read_u8_with_mode(self, from_op_read_mode);
                let to_op = self.get_reg_from_lookup_tab1(t).0;

                // Print before writing so we can see the old value
                if debug {
                    print!("LOAD {} with ", to_op);
                }

                to_op.write_u8(self, from_val);

                if debug {
                    println!("{}", from_op);
                }

                // If either operand is (HL), the cycle-count doubles
                // TODO(PT): Unit-test cycle counts
                let operand_names = [
                    self.get_reg_name_and_addressing_mode_from_lookup_tab1(f).0,
                    self.get_reg_name_and_addressing_mode_from_lookup_tab1(f).0,
                ];
                let cycle_count = if operand_names.contains(&RegisterName::HL) {
                    2
                } else {
                    1
                };
                InstrInfo::seq(1, cycle_count)
            }
            "00iii110" => {
                // LD [Reg], [u8]
                let dest = self.get_reg_from_lookup_tab1(i).0;
                let val = self.mmu.read(self.get_pc() + 1);
                dest.write_u8(&self, val);
                if debug {
                    println!("LD {dest} with 0x{val:02x}");
                }
                // TODO(PT): Update me when the operand is (HL)
                InstrInfo::seq(2, 2)
            }
            "10101iii" => {
                // XOR [Reg]
                let (operand, read_mode) = self.get_reg_from_lookup_tab1(i);
                let reg_a = self.reg(RegisterName::A);

                if debug {
                    print!("{reg_a} ^= {operand}\t");
                }

                let val = reg_a.read_u8(&self) ^ operand.read_u8_with_mode(&self, read_mode);
                reg_a.write_u8(&self, val);

                if debug {
                    println!("Result: {reg_a}");
                }

                self.set_flags(true, false, false, false);
                // TODO(PT): Update me with the cycle count for (HL)
                InstrInfo::seq(1, 1)
            }
            "00ii0001" => {
                // LD Reg16, u16
                let dest = self.get_reg_from_lookup_tab2(i).0;
                let val = self.mmu.read_u16(self.get_pc() + 1);

                if debug {
                    println!("LD {dest} with 0x{val:02x}");
                }

                dest.write_u16(&self, val);
                InstrInfo::seq(3, 3)
            }
            "00ii0010" => {
                // LD (Reg16), A
                let src = self.reg(RegisterName::A);
                let (dest, dest_addressing_mode) = self.get_reg_from_lookup_tab3(i);

                if debug {
                    println!("LOAD {dest}{dest_addressing_mode} with {src}");
                }

                dest.write_u8_with_mode(&self, dest_addressing_mode, src.read_u8(&self));

                InstrInfo::seq(1, 2)
            }
            "00ii1010" => {
                // LD A, [MemOp]
                let dest = self.reg(RegisterName::A);
                let (src, src_addressing_mode) = self.get_reg_from_lookup_tab3(i);

                if debug {
                    println!("LOAD {dest} with {src}{src_addressing_mode}");
                }

                dest.write_u8(&self, src.read_u8_with_mode(&self, src_addressing_mode));

                InstrInfo::seq(1, 2)
            }
            "001ii000" => {
                // JR N{Flag}, s8
                let cond = match i {
                    0 => FlagCondition::NotZero,
                    1 => FlagCondition::Zero,
                    2 => FlagCondition::NotCarry,
                    3 => FlagCondition::Carry,
                    _ => panic!("Invalid index"),
                };
                if self.is_flag_condition_met(cond) {
                    // TODO(PT): Refactor with JR s8?
                    let rel_target = self.mmu.read(self.get_pc() + 1) as i8;
                    if debug {
                        println!("JR {cond} +{:02x};\t(taken)", rel_target);
                    }
                    // Add 2 to PC before doing the relative target, as
                    // this instruction is 2 bytes wide
                    let mut pc = self.get_pc();
                    pc += 2;
                    pc = ((pc as i16) + rel_target as i16) as u16;
                    self.set_pc(pc);
                    InstrInfo::jump(2, 3)
                } else {
                    if debug {
                        println!("JR {cond}\t(not taken)");
                    }
                    InstrInfo::seq(2, 2)
                }
            }
            "111w0000" => {
                // LD (0xff00 + u8), A | LD A, (0xff00 + u8)
                let write_to_a = w == 1;
                let a = self.reg(RegisterName::A);
                let off_u8 = self.mmu.read(pc + 1);
                let address = 0xff00u16 + (off_u8 as u16);

                if debug {
                    if write_to_a {
                        println!("LD {a} with 0xff00 + {off_u8:02x}");
                    } else {
                        println!("LD (0xff00 + {off_u8:02x}) with {a}");
                    }
                }

                if write_to_a {
                    a.write_u8(&self, self.mmu.read(address));
                } else {
                    self.mmu.write(address, a.read_u8(&self));
                }
                InstrInfo::seq(2, 3)
            }
            "111w0010" => {
                // LD (0xff00 + C), A | LD A, (0xf00 + C)
                let write_to_a = w == 1;
                let a = self.reg(RegisterName::A);
                let c = self.reg(RegisterName::C);
                let address = 0xff00u16 + c.read_u8(&self) as u16;

                if debug {
                    if write_to_a {
                        println!("LD {a} with (0xff00 + {c})");
                    } else {
                        println!("LD (0xff00 + {c}) with {a}");
                    }
                }
                if write_to_a {
                    a.write_u8(&self, self.mmu.read(address));
                } else {
                    self.mmu.write(address, a.read_u8(&self));
                }
                InstrInfo::seq(1, 2)
            }
            "11ii0p01" => {
                // PUSH RegPair | POP RegPair
                let is_push = p == 1;

                let reg_name = match i {
                    0 => RegisterName::BC,
                    1 => RegisterName::DE,
                    2 => RegisterName::HL,
                    3 => RegisterName::AF,
                    _ => panic!("Invalid index"),
                };
                let reg = self.reg(reg_name);
                if is_push {
                    if debug {
                        println!("PUSH from {reg}");
                    }
                    self.push_u16(reg.read_u16(&self));
                    InstrInfo::seq(1, 4)
                } else {
                    if debug {
                        println!("POP into {reg}");
                    }
                    reg.write_u16(&self, self.pop_u16());
                    InstrInfo::seq(1, 3)
                }
            }
            "000c0111" => {
                // RLC A | RL A
                let is_rlc = c == 0;
                // Throw away the instruction info since this opcode has its own timings
                self.rlc_or_rl(self.reg(RegisterName::A), AddressingMode::Read, is_rlc);
                // The Z flag may have been set above, but this variant always clear it
                self.update_flag(FlagUpdate::Zero(false));
                InstrInfo::seq(1, 1)
            }
            "00ii0011" => {
                // INC Reg16
                let dest = self.get_reg_from_lookup_tab2(i).0;

                if debug {
                    println!("INC {dest}");
                }

                dest.write_u16(&self, dest.read_u16(&self) + 1);
                InstrInfo::seq(1, 2)
            }
            "111c1010" => {
                let load_into_a = c == 1;
                let a = self.reg(RegisterName::A);
                let address = self.mmu.read_u16(self.get_pc() + 1);
                let deref_value = self.mmu.read(address);

                if load_into_a {
                    if debug {
                        println!("LD {a}, ({:04x})[{:04x}]", address, deref_value);
                    }
                    a.write_u8(&self, deref_value);
                } else {
                    if debug {
                        println!("LD ({:04x})[{:04x}], {a}", address, deref_value);
                    }
                    self.mmu.write(address, a.read_u8(&self));
                }
                InstrInfo::seq(3, 4)
            }
            "10010iii" => {
                // SUB Reg8
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                let a = self.reg(RegisterName::A);
                let prev_a_val = a.read_u8(&self);

                if debug {
                    println!("SUB {op} from {a}\t");
                }

                let op_val = op.read_u8_with_mode(&self, read_mode);
                let prev = op.read_u8_with_mode(&self, read_mode);
                let (result, did_underflow) = a.read_u8(&self).overflowing_sub(op_val);

                a.write_u8(&self, result);
                self.update_flag(FlagUpdate::Zero(result == 0));
                self.update_flag(FlagUpdate::Subtract(true));
                // Underflow into the high nibble?
                self.update_flag(FlagUpdate::HalfCarry((prev_a_val & 0xf) < (result & 0xf)));
                // Underflow into next byte?
                self.update_flag(FlagUpdate::Carry(did_underflow));

                // TODO(PT): Should be 2 for (HL)
                InstrInfo::seq(1, 1)
            }
            _ => {
                println!("<0x{:02x} is unimplemented>", instruction_byte);
                self.print_regs();
                //panic!("Unimplemented opcode")
                InstrInfo::seq(0, 0)
            }
        }
    }

    fn push_u16(&self, val: u16) {
        let current_sp = self.reg(RegisterName::SP).read_u16(&self);
        let byte_count = mem::size_of::<u16>();
        // Decrement SP to account for push
        let new_stack_pointer = current_sp - (byte_count as u16);
        self.reg(RegisterName::SP)
            .write_u16(&self, new_stack_pointer);

        // Write the data where we just reserved space above the new SP
        let stack_storage = new_stack_pointer;
        self.mmu.write_u16(stack_storage, val);
    }

    fn pop_u16(&self) -> u16 {
        let sp = self.reg(RegisterName::SP);
        let byte_count = mem::size_of::<u16>() as u16;
        let current_sp = sp.read_u16(&self);
        let val = self.mmu.read_u16(current_sp);
        // Increment SP past the data that was just popped
        sp.write_u16(&self, current_sp + byte_count);
        val
    }

    pub fn step(&mut self) -> InstrInfo {
        let pc = self.get_pc();
        let info = self.decode(pc);
        if let Some(pc_increment) = info.pc_increment {
            assert_eq!(
                info.jumped, false,
                "Only expect to increment PC here when a jump was not taken"
            );
            let pc_reg = self.reg(RegisterName::PC);
            pc_reg.write_u16(self, pc + pc_increment);
        }
        info
    }
}

#[cfg(test)]
mod tests {
    use std::{cell::RefCell, rc::Rc};

    use crate::{
        cpu::{AddressingMode, Flag, FlagCondition, FlagUpdate, RegisterName},
        mmu::{Mmu, Ram},
    };

    use super::CpuState;

    // Notably, this is missing a PPU
    struct CpuTestSystem {
        pub mmu: Rc<Mmu>,
        pub cpu: RefCell<CpuState>,
    }

    impl CpuTestSystem {
        pub fn new(mmu: Rc<Mmu>, cpu: CpuState) -> Self {
            Self {
                mmu,
                cpu: RefCell::new(cpu),
            }
        }
    }

    fn get_system() -> CpuTestSystem {
        let ram = Rc::new(Ram::new(0, 0xffff));
        let mmu = Rc::new(Mmu::new(vec![ram]));
        let mut cpu = CpuState::new(Rc::clone(&mmu));
        cpu.enable_debug();
        CpuTestSystem::new(mmu, cpu)
    }

    /* Machinery tests */

    #[test]
    fn test_read_u8() {
        // Given memory initialised to zeroes
        let gb = get_system();
        let mmu = gb.mmu;
        mmu.write(20, 0xff);
        assert_eq!(mmu.read(20), 0xff);
        // And the surrounding values are still zero
        assert_eq!(mmu.read(19), 0x00);
        assert_eq!(mmu.read(21), 0x00);
    }

    #[test]
    fn test_read_u16() {
        // Given memory initialised to zeroes
        let gb = get_system();
        let mmu = gb.mmu;
        // And a u16 stored across two bytes
        mmu.write(20, 0x0d);
        mmu.write(21, 0x0c);
        // When I read the u16
        // Then its little endian representation is correctly parsed
        assert_eq!(mmu.read_u16(20), 0x0c0d);
        // And offset memory accesses look correct
        assert_eq!(mmu.read_u16(19), 0x0d00);
        assert_eq!(mmu.read_u16(21), 0x000c);
    }

    #[test]
    fn test_read_mem_hl() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.mmu.write(0xffcc, 0xab);
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0xcc);
        assert_eq!(cpu.reg(RegisterName::HL).read_u8(&cpu), 0xab);
    }

    #[test]
    fn test_write_mem_hl() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0xcc);

        let marker = 0x12;
        cpu.reg(RegisterName::HL).write_u8(&cpu, marker);
        // Then the write shows up directly in memory
        assert_eq!(cpu.mmu.read(0xffcc), marker);
        // And it shows up in the API for reading (HL)
        assert_eq!(cpu.reg(RegisterName::HL).read_u8(&cpu), marker);
    }

    #[test]
    fn test_wide_reg_read() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::B).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::C).write_u8(&cpu, 0xcc);
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0xffcc);
    }

    #[test]
    fn test_wide_reg_write() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::B).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::C).write_u8(&cpu, 0xcc);

        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xaabb);
        // Then the write shows up in both the individual registers and the wide register
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0xaabb);
        assert_eq!(cpu.reg(RegisterName::B).read_u8(&cpu), 0xaa);
        assert_eq!(cpu.reg(RegisterName::C).read_u8(&cpu), 0xbb);
    }

    #[test]
    fn test_wide_reg_addressing_mode_read() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given BC contains a pointer
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xaabb);
        // And this pointer contains some data
        cpu.mmu.write_u16(0xaabb, 0x23);
        // When we request an unadorned read
        // Then the memory is implicitly dereferenced
        assert_eq!(cpu.reg(RegisterName::BC).read_u8(&cpu), 0x23);
    }

    #[test]
    fn test_wide_reg_addressing_mode_deref() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given BC contains a pointer
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xaabb);
        // And this pointer contains some data
        cpu.mmu.write_u16(0xaabb, 0x23);

        // When we request a dereferenced read
        // Then we get the dereferenced data
        assert_eq!(
            cpu.reg(RegisterName::BC)
                .read_u8_with_mode(&cpu, AddressingMode::Deref),
            0x23
        );
        // And the contents of the registers are untouched
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0xaabb);
    }

    #[test]
    fn test_wide_reg_addressing_mode_deref_increment() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given HL contains a pointer
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xaabb);
        // And this pointer contains some data
        cpu.mmu.write_u16(0xaabb, 0x23);

        // When we request a dereferenced read and pointer increment
        // Then we get the dereferenced data
        assert_eq!(
            cpu.reg(RegisterName::HL)
                .read_u8_with_mode(&cpu, AddressingMode::DerefThenIncrement),
            0x23
        );
        // And the contents of the register are incremented
        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0xaabc);
    }

    #[test]
    fn test_wide_reg_addressing_mode_deref_decrement() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given HL contains a pointer
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xaabb);
        // And this pointer contains some data
        cpu.mmu.write_u16(0xaabb, 0x23);

        // When we request a dereferenced read and pointer decrement
        // Then we get the dereferenced data
        assert_eq!(
            cpu.reg(RegisterName::HL)
                .read_u8_with_mode(&cpu, AddressingMode::DerefThenDecrement),
            0x23
        );
        // And the contents of the register are decremented
        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0xaaba);
    }

    #[test]
    fn test_wide_reg_write_u8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given BC contains a pointer
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xaabb);
        // And this pointer contains some data
        cpu.mmu.write_u16(0xaabb, 0x23);
        // When we request an unadorned write
        cpu.reg(RegisterName::BC).write_u8(&cpu, 0x14);
        // Then the data the pointer points to has been overwritten
        assert_eq!(cpu.reg(RegisterName::BC).read_u8(&cpu), 0x14);
    }

    /* Instructions tests */

    /* DEC instruction tests */

    #[test]
    fn test_dec_reg() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let opcode_to_registers = [
            (0x05, RegisterName::B),
            (0x0d, RegisterName::C),
            (0x15, RegisterName::D),
            (0x1d, RegisterName::E),
            (0x25, RegisterName::H),
            (0x2d, RegisterName::L),
            (0x3d, RegisterName::A),
        ];
        for (opcode, register) in opcode_to_registers {
            cpu.mmu.write(0, opcode);

            // Given the register contains 5
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 5);
            cpu.step();
            assert_eq!(cpu.reg(register).read_u8(&cpu), 4);
            assert_eq!(cpu.is_flag_set(Flag::Zero), false);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

            // Given the register contains 0 (underflow)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 0);
            cpu.step();
            assert_eq!(cpu.reg(register).read_u8(&cpu), 0xff);
            assert_eq!(cpu.is_flag_set(Flag::Zero), false);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);

            // Given the register contains 1 (zero)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 1);
            cpu.step();
            assert_eq!(cpu.reg(register).read_u8(&cpu), 0);
            assert_eq!(cpu.is_flag_set(Flag::Zero), true);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

            // Given the register contains 0xf0 (half carry)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 0xf0);
            cpu.step();
            assert_eq!(cpu.reg(register).read_u8(&cpu), 0xef);
            assert_eq!(cpu.is_flag_set(Flag::Zero), false);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
        }
    }

    #[test]
    fn test_dec_mem_hl() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Given the memory pointed to by HL contains 0xf0
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0xcc);
        cpu.reg(RegisterName::HL).write_u8(&cpu, 0xf0);
        // When the CPU runs a DEC (HL) instruction
        // TODO(PT): Check cycle count here
        cpu.mmu.write(0, 0x35);
        cpu.step();
        // Then the memory has been decremented
        assert_eq!(cpu.reg(RegisterName::HL).read_u8(&cpu), 0xef);
        // And the flags are set correctly
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
    }

    /* Jump instructions */

    #[test]
    fn test_jmp() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.mmu.write(0, 0xc3);
        // Little endian branch target
        cpu.mmu.write(1, 0xfe);
        cpu.mmu.write(2, 0xca);
        cpu.step();
        assert_eq!(cpu.get_pc(), 0xcafe);
    }

    /* LD DstType1, U8 */

    #[test]
    fn test_ld_reg_u8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let opcode_to_registers = [
            (0x06, RegisterName::B),
            (0x0e, RegisterName::C),
            (0x16, RegisterName::D),
            (0x1e, RegisterName::E),
            (0x26, RegisterName::H),
            (0x2e, RegisterName::L),
            (0x3e, RegisterName::A),
        ];
        for (opcode, register) in opcode_to_registers {
            cpu.set_pc(0);
            let marker = 0xab;
            cpu.mmu.write(0, opcode);
            cpu.mmu.write(1, marker);

            // Given the register contains data other than the marker
            cpu.reg(register).write_u8(&cpu, 0xff);
            cpu.step();
            assert_eq!(cpu.reg(register).read_u8(&cpu), marker);
        }
    }

    #[test]
    fn test_ld_mem_hl_u8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // Given the memory pointed to by HL contains 0xf0
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0xcc);
        cpu.reg(RegisterName::HL).write_u8(&cpu, 0xf0);

        // When the CPU runs a LD (HL), u8 instruction
        // TODO(PT): Check cycle count here
        cpu.mmu.write(0, 0x36);
        cpu.mmu.write(1, 0xaa);
        cpu.step();
        // Then the memory has been assigned
        assert_eq!(cpu.reg(RegisterName::HL).read_u8(&cpu), 0xaa);
    }

    /* Load instruction tests */

    #[test]
    fn test_ld_b_c() {
        // Given a LD B, C instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let marker = 0xca;
        cpu.reg(RegisterName::C).write_u8(&cpu, marker);
        cpu.reg(RegisterName::B).write_u8(&cpu, 0x00);
        cpu.mmu.write(0, 0x41);
        cpu.step();
        // Then the register has been loaded
        assert_eq!(cpu.reg(RegisterName::B).read_u8(&cpu), marker);
    }

    #[test]
    fn test_ld_l_l() {
        // Given a LD L, L no-op instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let marker = 0xfd;
        cpu.reg(RegisterName::L).write_u8(&cpu, marker);
        cpu.mmu.write(0, 0x6d);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::L).read_u8(&cpu), marker);
    }

    #[test]
    fn test_ld_c_hl() {
        // Given an LD C, (HL) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0xcc);
        let marker = 0xdd;
        cpu.reg(RegisterName::HL).write_u8(&cpu, marker);
        cpu.mmu.write(0, 0x4e);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::C).read_u8(&cpu), marker);
    }

    #[test]
    fn test_ld_hl_a() {
        // Given an LD (HL), A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let marker = 0xaf;
        cpu.reg(RegisterName::A).write_u8(&cpu, marker);
        cpu.reg(RegisterName::H).write_u8(&cpu, 0x11);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0x22);
        cpu.mmu.write(0, 0x77);
        cpu.step();
        assert_eq!(cpu.mmu.read(0x1122), marker);
    }

    #[test]
    fn test_ld_h_hl() {
        // Given a LD H, (HL) no-op instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // Set up some sentinel data at the address HL will point to after the instruction
        // TODO(PT): Replace markers with a random u8?
        cpu.mmu.write(0xbb22, 0x33);

        cpu.reg(RegisterName::H).write_u8(&cpu, 0x11);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0x22);
        cpu.mmu.write(0x1122, 0xbb);
        cpu.mmu.write(0, 0x66);
        cpu.step();
        // Then the memory load has been applied
        assert_eq!(cpu.reg(RegisterName::H).read_u8(&cpu), 0xbb);
        // And dereferencing (HL) now accesses 0xbb22
        assert_eq!(cpu.reg(RegisterName::HL).read_u8(&cpu), 0x33);
    }

    /* XOR [Reg] */

    #[test]
    fn test_xor_b() {
        // Given a XOR B instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0b1110);
        cpu.reg(RegisterName::B).write_u8(&cpu, 0b0111);

        cpu.mmu.write(0, 0xa8);
        cpu.step();

        // Then the XOR has been applied and stored in A
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0b1001);
    }

    #[test]
    fn test_xor_mem_hl() {
        // Given a XOR (HL) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0b1110);

        cpu.reg(RegisterName::H).write_u8(&cpu, 0x11);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0x22);
        cpu.reg(RegisterName::HL).write_u8(&cpu, 0b0111);

        cpu.mmu.write(0, 0xae);
        cpu.step();

        // Then the XOR has been applied and stored in A
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0b1001);
    }

    /* LD Dst16, u16 */

    #[test]
    fn test_ld_dst16_u16_bc() {
        // Given an LD BC, u16 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And B and C contain some data
        cpu.reg(RegisterName::B).write_u8(&cpu, 0x33);
        cpu.reg(RegisterName::C).write_u8(&cpu, 0x44);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x01);
        cpu.mmu.write_u16(1, 0xcafe);
        cpu.step();

        // Then the write has been applied to the registers
        assert_eq!(cpu.reg(RegisterName::B).read_u8(&cpu), 0xca);
        assert_eq!(cpu.reg(RegisterName::C).read_u8(&cpu), 0xfe);
        // And the write shows up in the wide register
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0xcafe);
    }

    #[test]
    fn test_ld_dst16_u16_sp() {
        // Given an LD SP, u16 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And B and C contain some data
        // And SP contains some data
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xffaa);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x31);
        cpu.mmu.write_u16(1, 0xcafe);
        cpu.step();

        // Then the write has been applied to the stack pointer
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xcafe);
    }

    /* LD A, [Op16] */

    #[test]
    fn test_ld_a_op16() {
        // Given an LD A, (BC) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And B and C contain some data
        cpu.reg(RegisterName::B).write_u8(&cpu, 0x33);
        cpu.reg(RegisterName::C).write_u8(&cpu, 0x44);

        // And the address pointed to by BC contains some data
        cpu.mmu.write(0x3344, 0xfa);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x0a);
        cpu.step();

        // Then the contents of BC have not been modified
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0x3344);
        // And the data has been copied to A
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xfa);
        // And the memory has not been touched
        assert_eq!(cpu.mmu.read(0x3344), 0xfa);
    }

    #[test]
    fn test_ld_a_hl_plus() {
        // Given an LD A, (HL+) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And (HL) contains some data
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x01ff);

        // And the address pointed to by HL contains some data
        cpu.mmu.write(0x01ff, 0x56);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x2a);
        cpu.step();

        // Then the pointee has been copied to A
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x56);
        // And HL has been incremented
        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0x0200);
        // And the memory has not been touched
        assert_eq!(cpu.mmu.read(0x01ff), 0x56);
    }

    /* LD (Reg16), A */

    #[test]
    fn test_ld_bc_deref_a() {
        // Given an LD (BC), A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And (BC) contains a pointer
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xabcd);
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x11);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x02);
        cpu.step();

        // Then the data in A has been copied to the pointee
        assert_eq!(cpu.mmu.read(0xabcd), 0x11);
        // And the data shows up when dereferencing the register
        assert_eq!(cpu.reg(RegisterName::BC).read_u8(&cpu), 0x11);
        // And the contents of A have been left untouched
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x11);
    }

    #[test]
    fn test_ld_hl_minus_a() {
        // Given an LD (HL-), A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And HL contains a pointer
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xabcd);
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x11);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x32);
        cpu.step();

        // Then the data in A has been copied to the pointer
        assert_eq!(cpu.mmu.read(0xabcd), 0x11);
        // And HL has been decremented
        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0xabcc);
    }

    /* INC Reg8 */

    #[test]
    fn test_inc_reg8() {
        // Given an INC A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let params = [
            (0x05, 0x06, vec![]),
            (0xff, 0x00, vec![Flag::Zero, Flag::HalfCarry]),
        ];
        for (val, expected_incr, expected_flags) in params {
            // And A contains some value
            cpu.reg(RegisterName::A).write_u8(&cpu, val);
            // When the CPU runs the instruction
            cpu.reg(RegisterName::PC).write_u16(&cpu, 0x00);
            cpu.mmu.write(0, 0x3c);
            cpu.step();
            // Then the contents of A have been incremented
            assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), expected_incr);
            // And the flags are set correctly
            for flag in [Flag::Zero, Flag::Subtract, Flag::HalfCarry, Flag::Carry] {
                let expects_flag = expected_flags.contains(&flag);
                println!("Checking {flag:?}... for value {val}");
                assert_eq!(cpu.is_flag_set(flag), expects_flag);
            }
        }
    }

    /* JR N{FlagCond}, s8 */

    #[test]
    fn test_jr_nz_taken() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        let params = [
            // NotZero jump taken
            (
                0x20,
                FlagCondition::NotZero,
                FlagUpdate::Zero(false),
                Some(16),
            ),
            // NotZero jump not taken
            (0x20, FlagCondition::NotZero, FlagUpdate::Zero(true), None),
            // Zero jump taken
            (0x28, FlagCondition::Zero, FlagUpdate::Zero(true), Some(-8)),
            // Zero jump not taken
            (0x28, FlagCondition::Zero, FlagUpdate::Zero(false), None),
            // NotCarry jump taken
            (
                0x30,
                FlagCondition::NotCarry,
                FlagUpdate::Carry(false),
                Some(100),
            ),
            // NotCarry jump not taken
            (0x30, FlagCondition::NotCarry, FlagUpdate::Carry(true), None),
            // Carry jump taken
            (
                0x38,
                FlagCondition::Carry,
                FlagUpdate::Carry(true),
                Some(-100),
            ),
            // Carry jump not taken
            (0x38, FlagCondition::Carry, FlagUpdate::Carry(false), None),
        ];
        for (opcode, taken_cond, tested_flag, maybe_expected_jump_off) in params {
            // Clear PC and flags from the last run
            let pc_base = 0x200;
            cpu.set_pc(pc_base);
            cpu.set_flags(false, false, false, false);
            cpu.update_flag(tested_flag);

            cpu.mmu.write(pc_base, opcode);
            if let Some(expected_jump_off) = maybe_expected_jump_off {
                cpu.mmu.write(pc_base + 1, expected_jump_off as u8);
            }

            cpu.step();

            if let Some(expected_jump_off) = maybe_expected_jump_off {
                // Should have jumped, PC should be adjusted based on the relative target
                assert_eq!(
                    cpu.get_pc() as i16,
                    (pc_base as i16) + expected_jump_off + 2
                );
            } else {
                // Should not have jumped, PC should be just after the JR instruction
                assert_eq!(cpu.get_pc(), pc_base + 2);
            }
        }
    }

    /* Bit B, Reg8 */

    #[test]
    fn test_bit_b_reg8() {
        // Given a BIT B, Reg8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let params = [
            (0x40, RegisterName::B, 0, FlagCondition::Zero),
            (0x40, RegisterName::B, 1, FlagCondition::NotZero),
            (0x5b, RegisterName::E, 0b10111, FlagCondition::Zero),
            (0x5b, RegisterName::E, 0b01000, FlagCondition::NotZero),
            (0x7f, RegisterName::A, 0b01111111, FlagCondition::Zero),
            (0x7f, RegisterName::A, 0b10000000, FlagCondition::NotZero),
        ];
        for (opcode, register, value, expected_flag) in params {
            // Clear PC and flags from the last run
            cpu.set_pc(0x0);
            cpu.update_flag(FlagUpdate::Zero(false));

            cpu.mmu.write(0, 0xcb);
            cpu.mmu.write(1, opcode);

            // Given the register contains the provided value
            cpu.reg(register).write_u8(&cpu, value);
            // When the CPU runs the instruction
            cpu.step();
            // Then the PC has been incremented past the wide instruction
            assert_eq!(cpu.get_pc(), 2);
            // And the zero flag is set correctly based on the input data
            assert!(cpu.is_flag_condition_met(expected_flag));
        }
    }

    #[test]
    fn test_bit_b_hl_deref() {
        // Given a BIT B, (HL) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And HL containes a pointer
        // And the pointer itself does not have its MSB set
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xfcfc);
        // And the pointee contains a value with its LSB set
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x01);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x46);
        cpu.step();

        // Then the Z flag is cleared
        assert!(cpu.is_flag_condition_met(FlagCondition::NotZero));

        // And when the pointee contains a value with its LSB unset
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x00);

        // When the CPU runs the instruction
        cpu.set_pc(0);
        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x46);
        cpu.step();

        // Then the Z flag is set
        assert!(cpu.is_flag_condition_met(FlagCondition::Zero));
    }

    /* LD (0xff00 + C), A */

    #[test]
    fn test_ld_deref_c_with_a() {
        // Given an LD (0xff00 + C), A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xaa);
        // And C contains a pointee offset
        cpu.reg(RegisterName::C).write_u8(&cpu, 0x55);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xe2);
        cpu.step();

        // Then the pointee has been updated with the contents of A
        assert_eq!(cpu.mmu.read(0xff55), 0xaa);
        // And A is untouched
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xaa);
        // And C is untouched
        assert_eq!(cpu.reg(RegisterName::C).read_u8(&cpu), 0x55);
    }

    /* LD A, (0xff00 + C) */

    #[test]
    fn test_ld_a_with_deref_c() {
        // Given an LD A, (0xff00 + C) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And 0xff11 contains some data
        cpu.mmu.write(0xff11, 0xbb);
        // And C contains 0x11
        cpu.reg(RegisterName::C).write_u8(&cpu, 0x11);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xf2);
        cpu.step();

        // Then A has been updated with the contents of the memory
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xbb);
        // And C is untouched
        assert_eq!(cpu.reg(RegisterName::C).read_u8(&cpu), 0x11);
        // And the memory is untouched
        assert_eq!(cpu.mmu.read(0xff11), 0xbb);
    }

    /* LD (0xff00 + u8), A */

    #[test]
    fn test_ld_deref_u8_with_a() {
        // Given an LD (0xff00 + u8), A, instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xaa);
        // And the pointee contains some data
        cpu.mmu.write(0xffcc, 0x66);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xe0);
        cpu.mmu.write(1, 0xcc);
        cpu.step();

        // Then the pointee has been updated with the contents of A
        assert_eq!(cpu.mmu.read(0xffcc), 0xaa);
        // And A is untouched
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xaa);
    }

    /* LD A, (0xff00 + u8) */

    #[test]
    fn test_ld_a_with_deref_u8() {
        // Given an LD A, (0xff00 + u8) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xaa);
        // And the pointee contains some data
        cpu.mmu.write(0xffcc, 0x66);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xf0);
        cpu.mmu.write(1, 0xcc);
        cpu.step();

        // Then A has been updated with the pointee
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x66);
        // And the pointee is untouched
        assert_eq!(cpu.mmu.read(0xffcc), 0x66);
    }

    /* CALL u16 */

    #[test]
    fn test_call_u16() {
        // Given a CALL u16 instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);
        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xcd);
        cpu.mmu.write_u16(1, 0x5566);
        cpu.step();

        // Then PC has been redirected to the jump target
        assert_eq!(cpu.get_pc(), 0x5566);

        // And the stack pointer has been decremented due to the return address
        // being stored on the stack
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffc);

        // And the return address is stored on the stack
        let return_pc = 3;
        assert_eq!(cpu.mmu.read_u16(sp), return_pc);
    }

    /* PUSH Reg16 */

    #[test]
    fn test_push_bc() {
        // Given a PUSH BC instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);
        // And BC contains some data
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0x5566);
        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xc5);
        cpu.step();

        // Then the stack pointer has been decremented
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffc);

        // And the value is stored on the stack
        assert_eq!(cpu.mmu.read_u16(sp), 0x5566);
    }

    #[test]
    fn test_push_af() {
        // Given a PUSH AF instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);
        // And AF contains some data
        cpu.reg(RegisterName::AF).write_u16(&cpu, 0x55b0);
        // And the individual A and F registers have been updated correctly
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x55);
        assert_eq!(cpu.reg(RegisterName::F).read_u8(&cpu), 0xb0);
        // And the F register change shows up in the flags APIs
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Carry));

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xf5);
        cpu.step();

        // Then the stack pointer has been decremented
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffc);

        // And the value is stored on the stack
        assert_eq!(cpu.mmu.read_u16(sp), 0x55b0);
    }

    /* POP Reg16 */

    #[test]
    fn test_pop_bc() {
        // Given a POP BC instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);

        // And the stack contains some data
        cpu.push_u16(0x5566);
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffc);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xc1);
        cpu.step();

        // Then the stack pointer has been incremented
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffe);

        // And the value has been read into BC
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0x5566);
    }

    #[test]
    fn test_pop_af() {
        // Given a POP AF instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);

        // And the stack contains some data
        cpu.push_u16(0x5510);
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffc);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xf1);
        cpu.step();

        // Then the stack pointer has been incremented
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffe);

        // And the value has been read into AF
        assert_eq!(cpu.reg(RegisterName::AF).read_u16(&cpu), 0x5510);
        // And the value in F shows up in the flags APIs
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    #[test]
    fn test_rlc() {
        // Given an RLC A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x85);
        cpu.set_flags(true, true, true, false);

        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x07);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.cycle_count, 2);
        assert_eq!(instr_info.instruction_size, 2);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x0b);
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    #[test]
    fn test_rlc_z_flag() {
        // Given an RLC A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And the result is zero
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);
        // And the Z flag is previously unset
        cpu.update_flag(FlagUpdate::Zero(false));

        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x07);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.cycle_count, 2);
        assert_eq!(instr_info.instruction_size, 2);

        // Then the Z flag has been set
        assert!(cpu.is_flag_set(Flag::Zero));
    }

    #[test]
    fn test_rl() {
        // Given an RL A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x95);
        cpu.set_flags(true, true, true, true);

        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x17);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.cycle_count, 2);
        assert_eq!(instr_info.instruction_size, 2);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x2b);
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    #[test]
    fn test_rlc_a() {
        // Given an RLC A instruction (in the main opcode table)
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And the result is zero
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x85);
        // And the Z flag is previously unset
        cpu.update_flag(FlagUpdate::Zero(false));

        cpu.mmu.write(0, 0x07);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.cycle_count, 1);
        assert_eq!(instr_info.instruction_size, 1);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x0b);
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    #[test]
    fn test_rlc_a_z_flag() {
        // Given an RLC A instruction (in the main opcode table)
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And the result is zero
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);
        // And the Z flag is previously unset
        cpu.update_flag(FlagUpdate::Zero(false));

        cpu.mmu.write(0, 0x07);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.cycle_count, 1);
        assert_eq!(instr_info.instruction_size, 1);

        // Then the Z flag remains unset, even though the result was zero
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    /* INC Reg16 */

    #[test]
    fn test_inc_bc() {
        // Given an INC BC instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::BC).write_u16(&cpu, 0xfa);

        cpu.mmu.write(0, 0x03);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.instruction_size, 1);
        assert_eq!(instr_info.cycle_count, 2);

        // And the BC register has been incremented
        assert_eq!(cpu.reg(RegisterName::BC).read_u16(&cpu), 0xfb);
    }

    /* RET */
    #[test]
    fn test_ret() {
        // Given a RET instruction
        // And there is a stack set up
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);

        // And the stack contains some data
        cpu.push_u16(0x5566);
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffc);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xc9);
        let instr_info = cpu.step();
        // Then the instruction size and timings are correct
        assert_eq!(instr_info.instruction_size, 1);
        assert_eq!(instr_info.cycle_count, 4);

        // And the stack pointer has been incremented
        let sp = cpu.reg(RegisterName::SP).read_u16(&cpu);
        assert_eq!(sp, 0xfffe);

        // And the value has been read into PC
        assert_eq!(cpu.reg(RegisterName::PC).read_u16(&cpu), 0x5566);
        // And it's indicated that a jump occurred
        assert_eq!(instr_info.pc_increment, None);
        assert!(instr_info.jumped);
    }

    /* JR i8 */

    #[test]
    fn test_jr_i8() {
        // Given a JR -6 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.mmu.write(0x20, 0x18);
        cpu.mmu.write(0x21, (-6i8 as u8));
        cpu.set_pc(0x20);

        let instr_info = cpu.step();
        assert_eq!(cpu.get_pc(), 0x1c);
        // And it's indicated that a jump occurred
        assert_eq!(instr_info.pc_increment, None);
        assert!(instr_info.jumped);
        assert_eq!(instr_info.instruction_size, 2);
        assert_eq!(instr_info.cycle_count, 3);
    }

    /* CP u8 */
    #[test]
    fn test_cp_u8() {
        // Given a CP u8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3c);
        cpu.mmu.write(0, 0xfe);
        cpu.mmu.write(1, 0x3c);
        let instr_info = cpu.step();
        assert_eq!(instr_info.instruction_size, 2);
        assert_eq!(instr_info.cycle_count, 2);
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* CP (HL) */
    #[test]
    fn test_cp_hl_deref() {
        // Given a CP (HL) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3c);
        cpu.mmu.write(0, 0xbe);
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xffaa);
        cpu.mmu.write(0xffaa, 0x3c);
        let instr_info = cpu.step();
        assert_eq!(instr_info.instruction_size, 1);
        assert_eq!(instr_info.cycle_count, 2);
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* LD (u16), A */

    #[test]
    fn test_ld_deref_u16_with_a() {
        // Given an LD (u16), A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xaa);
        // And the pointee contains some data
        let address = 0xffee;
        cpu.mmu.write(address, 0xbb);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xea);
        cpu.mmu.write_u16(1, address);
        let instr_info = cpu.step();
        assert_eq!(instr_info.instruction_size, 3);
        assert_eq!(instr_info.cycle_count, 4);

        // Then the pointee has been updated with the contents of A
        assert_eq!(cpu.mmu.read(address), 0xaa);
        // And A is untouched
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xaa);
    }

    /* LD A, (u16) */

    #[test]
    fn test_ld_a_with_deref_u16() {
        // Given an LD A, (u16) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And A contains some data
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xaa);
        // And the pointee contains some data
        let address = 0xffee;
        cpu.mmu.write(address, 0xbb);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0xfa);
        cpu.mmu.write_u16(1, address);
        let instr_info = cpu.step();
        assert_eq!(instr_info.instruction_size, 3);
        assert_eq!(instr_info.cycle_count, 4);

        // Then A has been updated with the contents of the pointee
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xbb);
        // And the pointee is untouched
        assert_eq!(cpu.mmu.read(address), 0xbb);
    }

    /* SUB Reg8 */

    #[test]
    fn test_sub_reg8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.mmu.write(0, 0x90);

        // Given B contains 1
        cpu.reg(RegisterName::B).write_u8(&cpu, 1);

        // Given A contains 5
        cpu.reg(RegisterName::A).write_u8(&cpu, 5);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 4);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);

        // Given the register contains 0 (underflow)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xff);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
        assert_eq!(cpu.is_flag_set(Flag::Carry), true);

        // Given the register contains 1 (zero)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 1);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0);
        assert_eq!(cpu.is_flag_set(Flag::Zero), true);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);

        // Given the register contains 0xf0 (half carry)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xf0);
        cpu.step();
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xef);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);
    }
}
