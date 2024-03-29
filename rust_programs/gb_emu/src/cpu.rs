use alloc::{boxed::Box, collections::BTreeMap, format, rc::Rc, string::String, vec::Vec};
use core::{
    cell::RefCell,
    fmt::{self, Debug, Display},
    mem,
};

#[cfg(not(feature = "use_std"))]
use axle_rt::{print, println};

use bitmatch::bitmatch;

use crate::{
    gameboy::GameBoyHardwareProvider,
    interrupts::{InterruptController, InterruptType},
    mmu::Mmu,
};

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

#[derive(Debug, PartialEq, Copy, Clone)]
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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
    // TODO(PT): Remove stored reference to MMU and use GameBoyHardwareProvider instead
    mmu: Rc<Mmu>,
    debug_enabled: bool,
    pub is_halted: bool,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum RegisterName {
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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

pub trait VariableStorage: Debug + Display {
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
    name: RegisterName,
    display_name: String,
    contents: RefCell<u8>,
}

impl CpuRegister {
    fn new(name: RegisterName) -> Self {
        Self {
            name,
            display_name: format!("{name}"),
            contents: RefCell::new(0),
        }
    }
}

impl VariableStorage for CpuRegister {
    fn display_name(&self) -> &str {
        &self.display_name
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
        let val = match self.name {
            // The flags register only supports the high nibble
            RegisterName::F => val & 0xf0,
            _ => val,
        };
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}({:02x})", self.display_name, *self.contents.borrow())
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let name = match self {
            AddressingMode::Read => "",
            AddressingMode::Deref => "[]",
            AddressingMode::DerefThenIncrement => "[+]",
            AddressingMode::DerefThenDecrement => "[-]",
        };
        write!(f, "{}", name)
    }
}

impl CpuState {
    pub fn new(mmu: Rc<Mmu>) -> Self {
        // TODO(PT): Provide an 'operation' flag to instruction decoding
        // If the operation is 'decode', print the decoded instruction without running it
        // If the operation is 'execute', run the instruction
        // This allows us to keep just a single decoding layer
        let mut operands: BTreeMap<RegisterName, Box<dyn VariableStorage>> = BTreeMap::new();

        // 8-bit operands
        operands.insert(RegisterName::B, Box::new(CpuRegister::new(RegisterName::B)));
        operands.insert(RegisterName::C, Box::new(CpuRegister::new(RegisterName::C)));
        operands.insert(RegisterName::D, Box::new(CpuRegister::new(RegisterName::D)));
        operands.insert(RegisterName::E, Box::new(CpuRegister::new(RegisterName::E)));
        operands.insert(RegisterName::H, Box::new(CpuRegister::new(RegisterName::H)));
        operands.insert(RegisterName::L, Box::new(CpuRegister::new(RegisterName::L)));
        operands.insert(RegisterName::A, Box::new(CpuRegister::new(RegisterName::A)));
        operands.insert(RegisterName::F, Box::new(CpuRegister::new(RegisterName::F)));

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
            is_halted: false,
        }
    }

    pub fn set_mock_bootrom_state(&mut self) {
        self.set_pc(0x0100);
        self.reg(RegisterName::SP).write_u16(&self, 0xfffe);
        self.reg(RegisterName::A).write_u8(&self, 0x01);
        self.reg(RegisterName::C).write_u8(&self, 0x13);
        self.reg(RegisterName::E).write_u8(&self, 0xd8);
        self.reg(RegisterName::H).write_u8(&self, 0x01);
        self.reg(RegisterName::L).write_u8(&self, 0x4d);
        self.set_flags(true, false, true, true);
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
        println!(
            "AF\t= ${:02x}{:02x} ({})",
            self.reg(RegisterName::A).read_u8(&self),
            self.reg(RegisterName::F).read_u8(&self),
            self.format_flags()
        );
        println!(
            "BC\t= ${:02x}{:02x}",
            self.reg(RegisterName::B).read_u8(&self),
            self.reg(RegisterName::C).read_u8(&self)
        );
        println!(
            "DE\t= ${:02x}{:02x}",
            self.reg(RegisterName::D).read_u8(&self),
            self.reg(RegisterName::E).read_u8(&self)
        );
        println!(
            "HL\t= ${:02x}{:02x}",
            self.reg(RegisterName::H).read_u8(&self),
            self.reg(RegisterName::L).read_u8(&self)
        );
        println!("SP\t= ${:04x}", self.reg(RegisterName::SP).read_u16(&self));
        println!("PC\t= ${:04x}", self.reg(RegisterName::PC).read_u16(&self));
    }

    fn set_flags(&self, z: bool, n: bool, h: bool, c: bool) {
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

    fn sub8_update_flags(&self, a: u8, b: u8, ignored_flags: &[Flag]) -> u8 {
        if !ignored_flags.contains(&Flag::HalfCarry) {
            // HalfCarry if the second number's low nibble is larger than the first number's
            let half_carry_set = ((a & 0x0f).overflowing_sub((b & 0x0f)).0) & 0x10;
            self.update_flag(FlagUpdate::HalfCarry(half_carry_set != 0));
        }

        if !ignored_flags.contains(&Flag::Subtract) {
            self.update_flag(FlagUpdate::Subtract(true));
        }

        let (result, did_underflow) = a.overflowing_sub(b);

        if !ignored_flags.contains(&Flag::Carry) {
            // Carry if we're subtracting a larger number from a smaller one
            //self.update_flag(FlagUpdate::Carry(b > a));
            self.update_flag(FlagUpdate::Carry(did_underflow));
        }

        if !ignored_flags.contains(&Flag::Zero) {
            self.update_flag(FlagUpdate::Zero(result == 0));
        }

        result
    }

    fn add8_update_flags(&self, a: u8, b: u8, ignored_flags: &[Flag]) -> u8 {
        // HalfCarry if the second number's low nibble is larger than the first number's
        if !ignored_flags.contains(&Flag::HalfCarry) {
            let half_carry_flag =
                ((((a as u16) & 0xf).overflowing_add(((b as u16) & 0xf)).0) & 0x10) == 0x10;
            self.update_flag(FlagUpdate::HalfCarry(half_carry_flag));
        }

        if !ignored_flags.contains(&Flag::Subtract) {
            self.update_flag(FlagUpdate::Subtract(false));
        }

        let (result, did_overflow) = a.overflowing_add(b);

        if !ignored_flags.contains(&Flag::Carry) {
            self.update_flag(FlagUpdate::Carry(did_overflow));
        }

        if !ignored_flags.contains(&Flag::Zero) {
            self.update_flag(FlagUpdate::Zero(result == 0));
        }

        result
    }

    fn add8_with_carry_and_update_flags(&self, a: u8, b: u8) -> u8 {
        // Check for carry using 32bit arithmetic
        let a = a as u32;
        let b = b as u32;
        let carry = if self.is_flag_set(Flag::Carry) { 1 } else { 0 };

        let result = a.wrapping_add(b).wrapping_add(carry);
        let result_as_byte = result as u8;

        self.update_flag(FlagUpdate::Zero(result_as_byte == 0));
        self.update_flag(FlagUpdate::HalfCarry((a ^ b ^ result) & 0x10 != 0));
        self.update_flag(FlagUpdate::Carry(result & 0x100 != 0));
        self.update_flag(FlagUpdate::Subtract(false));

        result_as_byte
    }

    fn sub8_with_carry_and_update_flags(&self, a: u8, b: u8) -> u8 {
        // Check for carry using 32bit arithmetic
        let a = a as u32;
        let b = b as u32;
        let carry = if self.is_flag_set(Flag::Carry) { 1 } else { 0 };

        let result = a.wrapping_sub(b).wrapping_sub(carry);
        let result_as_byte = result as u8;

        self.update_flag(FlagUpdate::Zero(result_as_byte == 0));
        self.update_flag(FlagUpdate::HalfCarry((a ^ b ^ result) & 0x10 != 0));
        self.update_flag(FlagUpdate::Carry(result & 0x100 != 0));
        self.update_flag(FlagUpdate::Subtract(true));

        result_as_byte
    }

    fn sub16_skip_flags(&self, a: u16, b: u16) -> u16 {
        a.overflowing_sub(b).0
    }

    fn sub16_update_flags(&self, a: u16, b: u16) -> u16 {
        todo!()
    }

    fn add16_skip_flags(&self, a: u16, b: u16) -> u16 {
        a.overflowing_add(b).0
    }

    fn add16_update_flags(&self, a: u16, b: u16, ignored_flags: &[Flag]) -> u16 {
        // HalfCarry if the second number's low nibble is larger than the first number's
        if !ignored_flags.contains(&Flag::HalfCarry) {
            let hc = (a & 0xfff).overflowing_add(b & 0xfff).0 & 0x1000 == 0x1000;
            self.update_flag(FlagUpdate::HalfCarry(hc));
        }

        if !ignored_flags.contains(&Flag::Subtract) {
            self.update_flag(FlagUpdate::Subtract(false));
        }

        let (result, did_overflow) = a.overflowing_add(b);

        if !ignored_flags.contains(&Flag::Carry) {
            self.update_flag(FlagUpdate::Carry(did_overflow));
        }

        if !ignored_flags.contains(&Flag::Zero) {
            self.update_flag(FlagUpdate::Zero(result == 0));
        }

        result
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
        self.sub8_update_flags(a_val, val, &[]);
        InstrInfo::seq(1, 1)
    }

    fn call_addr(&self, instruction_size: u16, target: u16) {
        // Store the return address on the stack.
        // After the call completes,
        // return to the address after 1-byte opcode and 2-byte jump target
        self.push_u16(self.get_pc() + instruction_size);
        // Assign PC to the jump target
        self.set_pc(target);
    }

    pub fn call_interrupt_vector(&mut self, interrupt_type: InterruptType) {
        // TODO(PT): Unit test this
        let interrupt_vector = match interrupt_type {
            InterruptType::VBlank => 0x40,
            InterruptType::LCDStat => 0x48,
            InterruptType::Timer => 0x50,
            InterruptType::Serial => 0x58,
            InterruptType::Joypad => 0x60,
        };

        self.push_u16(self.get_pc());
        self.set_pc(interrupt_vector);
        self.set_halted(false);
    }

    fn rr_reg8(&self, reg: &dyn VariableStorage, addressing_mode: AddressingMode) {
        if self.debug_enabled {
            println!("RR {reg}");
        }

        let val = reg.read_u8_with_mode(&self, addressing_mode);

        let lsb = val & 0b1;
        // Copy the carry flag to the high bit
        let carry_copy = match self.is_flag_set(Flag::Carry) {
            true => 1,
            false => 0,
        };
        let new_val = (val >> 1) | (carry_copy << 7);

        reg.write_u8_with_mode(&self, addressing_mode, new_val);
        self.update_flag(FlagUpdate::Zero(new_val == 0));
        self.update_flag(FlagUpdate::Carry(lsb == 1));
        self.update_flag(FlagUpdate::HalfCarry(false));
        self.update_flag(FlagUpdate::Subtract(false));
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
                let bit = (contents & (1 << bit_to_test)) != 0;
                self.update_flag(FlagUpdate::Zero(!bit));
                self.update_flag(FlagUpdate::Subtract(false));
                self.update_flag(FlagUpdate::HalfCarry(true));
                // TODO(PT): Should be three cycles when (HL)
                2
            }
            "10bbbiii" => {
                // RES B, Reg8
                let bit_to_reset = b;
                let (reg, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("RES {bit_to_reset}, {reg}");
                }
                let mut contents = reg.read_u8_with_mode(&self, read_mode);
                contents &= !(1 << bit_to_reset);
                reg.write_u8_with_mode(&self, read_mode, contents);
                // TODO(PT): Should be 4 cycles when (HL)
                2
            }
            "000c0iii" => {
                // RLC Reg8 | RL Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                let is_rlc = c == 0;
                self.rlc_or_rl(reg, addressing_mode, is_rlc).cycle_count
            }
            "00110iii" => {
                // SWAP Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                let val = reg.read_u8_with_mode(&self, addressing_mode);
                if debug {
                    println!("SWAP {reg}");
                }
                let low_nibble = (val & 0x0f);
                let high_nibble = (val & 0xf0) >> 4;
                let result = (low_nibble << 4) | high_nibble;
                reg.write_u8(&self, result);
                self.set_flags(result == 0, false, false, false);
                // TODO(PT): Should be 4 cycles for (HL)
                2
            }
            "00111iii" => {
                // SRL Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                let val = reg.read_u8_with_mode(&self, addressing_mode);
                if debug {
                    println!("SRL {reg}");
                }

                // TODO(PT): Should be 4 cycles for (HL)
                let lsb = val & 0b1;
                let new_val = val >> 1;

                reg.write_u8_with_mode(&self, addressing_mode, new_val);
                self.update_flag(FlagUpdate::Zero(new_val == 0));
                self.update_flag(FlagUpdate::Carry(lsb == 1));
                self.update_flag(FlagUpdate::HalfCarry(false));
                self.update_flag(FlagUpdate::Subtract(false));
                2
            }
            "00011iii" => {
                // RR Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                // TODO(PT): Should be 4 cycles for (HL)
                self.rr_reg8(reg, addressing_mode);
                2
            }
            "00100iii" => {
                // SLA Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);

                if debug {
                    println!("SLA {reg}");
                }

                let val = reg.read_u8_with_mode(&self, addressing_mode);
                let msb = (val >> 7) & 0b1;
                let new_val = val << 1;

                reg.write_u8_with_mode(&self, addressing_mode, new_val);

                self.update_flag(FlagUpdate::Zero(new_val == 0));
                self.update_flag(FlagUpdate::Carry(msb == 1));
                self.update_flag(FlagUpdate::HalfCarry(false));
                self.update_flag(FlagUpdate::Subtract(false));

                // TODO(PT): Should be 4 cycles for (HL)
                2
            }
            "11bbbiii" => {
                // SET B, Reg8
                let bit_to_set = b;
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("SET {bit_to_set}, {reg}");
                }
                let contents = reg.read_u8_with_mode(&self, addressing_mode);
                let new_val = contents | (1 << bit_to_set);
                reg.write_u8_with_mode(&self, addressing_mode, new_val);
                // TODO(PT): Should be four cycles when (HL)
                2
            }
            "00101iii" => {
                // SRA Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("SRA {reg}");
                }
                let contents = reg.read_u8_with_mode(&self, addressing_mode);
                let lsb = contents & 0b1;
                let msb = contents >> 7;
                let new_contents = (contents >> 1) | (msb << 7);
                self.set_flags(new_contents == 0, false, false, lsb == 1);
                reg.write_u8_with_mode(&self, addressing_mode, new_contents);
                // TODO(PT): Should be four cycles when (HL)
                2
            }
            "00001iii" => {
                // RRC Reg8
                let (reg, addressing_mode) = self.get_reg_from_lookup_tab1(i);
                self.rrc_reg8(reg, addressing_mode);
                // TODO(PT): Should be four cycles when (HL)
                2
            }
            _ => {
                println!("<cb {:02x} is unimplemented>", instruction_byte);
                self.print_regs();
                //panic!("Unimplemented CB opcode")
                0
            }
        }
    }

    fn rrc_reg8(&self, reg: &dyn VariableStorage, addressing_mode: AddressingMode) {
        if self.debug_enabled {
            println!("RRC {reg}");
        }
        let contents = reg.read_u8_with_mode(&self, addressing_mode);
        let lsb = contents & 0b1;
        let new_contents = (contents >> 1) | (lsb << 7);
        self.set_flags(new_contents == 0, false, false, lsb == 1);
        reg.write_u8_with_mode(&self, addressing_mode, new_contents);
    }

    fn adc_a_u8(&self, val: u8) {
        let a = self.reg(RegisterName::A);
        let prev = a.read_u8(&self);
        let carry = if self.is_flag_set(Flag::Carry) { 1 } else { 0 };
        a.write_u8(&self, self.add8_with_carry_and_update_flags(prev, val));
    }

    #[bitmatch]
    fn decode(&mut self, pc: u16, system: &dyn GameBoyHardwareProvider) -> InstrInfo {
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
                if debug {
                    println!("HALT");
                }
                self.set_halted(true);
                Some(InstrInfo::seq(1, 4))
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
                let target = self.mmu.read_u16(self.get_pc() + 1);
                if debug {
                    println!("CALL 0x{target:04x}");
                }
                let instr_size = 3;
                self.call_addr(instr_size, target);
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
            0xd9 => {
                // RETI
                // TODO(PT): This should be restored to its previous state?
                system
                    .get_interrupt_controller()
                    .set_interrupts_globally_enabled();
                self.set_pc(self.pop_u16());
                if debug {
                    println!("RETI {:04x}", self.get_pc());
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
            0xf3 => {
                // DI
                let interrupt_controller = system.get_interrupt_controller();
                if debug {
                    println!("DI");
                }
                interrupt_controller.set_interrupts_globally_disabled();
                Some(InstrInfo::seq(1, 1))
            }
            0xfb => {
                // EI
                let interrupt_controller = system.get_interrupt_controller();
                if debug {
                    println!("EI");
                }
                interrupt_controller.set_interrupts_globally_enabled();
                Some(InstrInfo::seq(1, 1))
            }
            0x2f => {
                // CPL A
                let a = self.reg(RegisterName::A);
                if debug {
                    println!("CPL {a}");
                }
                a.write_u8(&self, !a.read_u8(&self));
                self.update_flag(FlagUpdate::Subtract(true));
                self.update_flag(FlagUpdate::HalfCarry(true));
                Some(InstrInfo::seq(1, 1))
            }
            0xe6 => {
                // AND u8
                let a = self.reg(RegisterName::A);
                let val = self.mmu.read(self.get_pc() + 1);
                if debug {
                    println!("AND {a}, {val:02x}");
                }

                let result = a.read_u8(&self) & val;
                a.write_u8(&self, result);
                self.update_flag(FlagUpdate::Zero(result == 0));
                self.update_flag(FlagUpdate::HalfCarry(true));
                self.update_flag(FlagUpdate::Carry(false));
                self.update_flag(FlagUpdate::Subtract(false));
                Some(InstrInfo::seq(2, 2))
            }
            0xc6 => {
                // ADD A, u8
                // TODO(PT): Refactor with ADD A, Reg8?
                let a = self.reg(RegisterName::A);
                let val = self.mmu.read(self.get_pc() + 1);

                if debug {
                    println!("ADD {a}, {val:02x}");
                }

                let prev_a_val = a.read_u8(&self);
                let result = self.add8_update_flags(prev_a_val, val, &[]);
                a.write_u8(&self, result);

                // TODO(PT): Cycle count should be 2 for (HL)
                Some(InstrInfo::seq(2, 2))
            }
            0xd6 => {
                // SUB A, u8
                // TODO(PT): Refactor with SUB Reg8
                let a = self.reg(RegisterName::A);
                let val = self.mmu.read(self.get_pc() + 1);
                let prev_a_val = a.read_u8(&self);

                if debug {
                    println!("SUB {val:02x} from {a}");
                }

                let result = self.sub8_update_flags(prev_a_val, val, &[]);
                a.write_u8(&self, result);

                // TODO(PT): Should be 2 for (HL)
                Some(InstrInfo::seq(2, 2))
            }
            0xe9 => {
                // JP HL
                let hl = self.reg(RegisterName::HL);
                let hl_val = hl.read_u16(&self);
                self.set_pc(hl_val);
                if debug {
                    println!("JP {hl}");
                }
                Some(InstrInfo::jump(1, 1))
            }
            0x1f => {
                // RRA
                self.rr_reg8(self.reg(RegisterName::A), AddressingMode::Read);
                // This variant always unsets the Z flag
                self.update_flag(FlagUpdate::Zero(false));
                Some(InstrInfo::seq(1, 1))
            }
            0xee => {
                // XOR A, u8
                let prev = self.reg(RegisterName::A).read_u8(&self);
                let val = self.mmu.read(self.get_pc() + 1);
                let new = prev ^ val;

                if debug {
                    println!("XOR {}, {val:02x}", self.reg(RegisterName::A));
                }

                self.reg(RegisterName::A).write_u8(&self, new);

                self.set_flags(new == 0, false, false, false);
                Some(InstrInfo::seq(2, 2))
            }
            0xce => {
                // ADC A, u8
                let a = self.reg(RegisterName::A);
                let val = self.mmu.read(self.get_pc() + 1);

                if debug {
                    println!("ADC {a}, {val:02x}");
                }

                self.adc_a_u8(val);

                Some(InstrInfo::seq(2, 2))
            }
            0x08 => {
                // LD (u16), SP
                let pointer_addr = self.mmu.read_u16(self.get_pc() + 1);
                let sp = self.reg(RegisterName::SP);

                if debug {
                    println!("LD ({pointer_addr:04x}) with {sp}");
                }

                self.mmu.write_u16(pointer_addr, sp.read_u16(&self));

                Some(InstrInfo::seq(3, 5))
            }
            0xf9 => {
                // LD SP, HL
                let hl = self.reg(RegisterName::HL);
                let sp = self.reg(RegisterName::SP);

                if debug {
                    println!("LD {sp} with {hl}");
                }

                sp.write_u16(&self, hl.read_u16(&self));

                Some(InstrInfo::seq(1, 2))
            }
            0x37 => {
                // SCF
                if debug {
                    println!("SCF");
                }
                self.update_flag(FlagUpdate::Subtract(false));
                self.update_flag(FlagUpdate::HalfCarry(false));
                self.update_flag(FlagUpdate::Carry(true));
                Some(InstrInfo::seq(1, 1))
            }
            0x3f => {
                // CCF
                if debug {
                    println!("CCF");
                }
                self.update_flag(FlagUpdate::Subtract(false));
                self.update_flag(FlagUpdate::HalfCarry(false));
                let prev_carry = self.is_flag_set(Flag::Carry);
                self.update_flag(FlagUpdate::Carry(!prev_carry));
                Some(InstrInfo::seq(1, 1))
            }
            0x0f => {
                // RRC A
                let a = self.reg(RegisterName::A);
                self.rrc_reg8(a, AddressingMode::Read);
                // This variant always unsets the Z flag
                self.update_flag(FlagUpdate::Zero(false));
                Some(InstrInfo::seq(1, 1))
            }
            0xe8 => {
                // ADD SP, i8
                let sp = self.reg(RegisterName::SP).read_u16(&self);
                let offset = self.mmu.read(self.get_pc() + 1);
                let signed_offset = offset as i8;

                if debug {
                    println!("ADD SP, {signed_offset}");
                }

                let sp_low_byte = (sp & 0xff) as u8;
                self.add8_update_flags(sp_low_byte, offset, &[]);

                let new_sp = match signed_offset > 0 {
                    true => sp + (offset as u16),
                    false => sp - (signed_offset.abs() as u16),
                };
                self.reg(RegisterName::SP).write_u16(&self, new_sp);

                // This instruction always unsets Z and N
                self.update_flag(FlagUpdate::Zero(false));
                self.update_flag(FlagUpdate::Subtract(false));

                Some(InstrInfo::seq(2, 4))
            }
            0xf8 => {
                // LD HL, SP+i8
                let hl = self.reg(RegisterName::HL);
                let sp = self.reg(RegisterName::SP);
                let sp_val = sp.read_u16(&self);
                let offset = self.mmu.read(self.get_pc() + 1);
                let signed_offset = offset as i8;

                if debug {
                    println!("ADD {hl}, {sp} + {signed_offset}");
                }

                let sp_low_byte = (sp_val & 0xff) as u8;
                self.add8_update_flags(sp_low_byte, offset, &[]);

                let result = match signed_offset > 0 {
                    true => sp_val + (offset as u16),
                    false => sp_val - (signed_offset.abs() as u16),
                };
                hl.write_u16(&self, result);

                // This instruction always unsets Z and N
                self.update_flag(FlagUpdate::Zero(false));
                self.update_flag(FlagUpdate::Subtract(false));

                Some(InstrInfo::seq(2, 3))
            }
            0xf6 => {
                // OR A, u8
                let a = self.reg(RegisterName::A);
                let val = self.mmu.read(self.get_pc() + 1);
                let a_val = a.read_u8(&self);
                let result = a_val | val;
                a.write_u8(&self, result);
                self.set_flags(result == 0, false, false, false);
                Some(InstrInfo::seq(2, 2))
            }
            0x27 => {
                // DAA
                // Ref: https://www.reddit.com/r/EmuDev/comments/4ycoix/a_guide_to_the_gameboys_halfcarry_flag/
                if debug {
                    println!("DAA");
                }

                let mut u = 0;
                let mut ra = self.reg(RegisterName::A).read_u8(&self);
                if self.is_flag_set(Flag::HalfCarry)
                    || (!self.is_flag_set(Flag::Subtract) && (ra & 0x0f) > 9)
                {
                    u = 6;
                }
                if self.is_flag_set(Flag::Carry) || (!self.is_flag_set(Flag::Subtract) && ra > 0x99)
                {
                    u |= 0x60;
                    self.update_flag(FlagUpdate::Carry(true));
                }

                let add_val = match self.is_flag_set(Flag::Subtract) {
                    true => (-(u as i8)) as u8,
                    false => u,
                };
                ra = ra.wrapping_add(add_val);
                self.update_flag(FlagUpdate::Zero(ra == 0));
                self.update_flag(FlagUpdate::HalfCarry(false));
                self.reg(RegisterName::A).write_u8(&self, ra);
                Some(InstrInfo::seq(1, 1))
            }
            0xde => {
                // SBC A, u8
                let a = self.reg(RegisterName::A);
                let val = system.get_mmu().read(self.get_pc() + 1);

                if debug {
                    println!("SBC {val:02x} from {a}");
                }

                let result = self.sub8_with_carry_and_update_flags(a.read_u8(&self), val);
                a.write_u8(&self, result);

                Some(InstrInfo::seq(2, 2))
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
                op.write_u8_with_mode(
                    &self,
                    read_mode,
                    self.add8_update_flags(prev, 1, &[Flag::Carry]),
                );
                // TODO(PT): Cycle count should be 3 for (HL)
                // TODO(PT): Should set Carry flag? CPU chart says no, why?
                InstrInfo::seq(1, 1)
            }
            "00iii101" => {
                // DEC Reg8
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    print!("DEC {op}\t");
                }
                let prev = op.read_u8_with_mode(&self, read_mode);
                op.write_u8_with_mode(
                    &self,
                    read_mode,
                    self.sub8_update_flags(prev, 1, &[Flag::Carry]),
                );

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
                // XOR Reg8
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

                self.set_flags(val == 0, false, false, false);
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

                let prev = dest.read_u16(&self);
                let new = prev.overflowing_add(1).0;
                dest.write_u16(&self, new);

                InstrInfo::seq(1, 2)
            }
            "111c1010" => {
                // LD A, (u16) | LD (u16), A
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

                if debug {
                    println!("SUB {op} from {a}\t");
                }

                let op_val = op.read_u8_with_mode(&self, read_mode);
                let result = self.sub8_update_flags(a.read_u8(&self), op_val, &[]);
                a.write_u8(&self, result);

                // TODO(PT): Should be 2 for (HL)
                InstrInfo::seq(1, 1)
            }
            "10000iii" => {
                // ADD A, Reg8
                let a = self.reg(RegisterName::A);
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);

                if debug {
                    println!("ADD {a}, {op}");
                }

                let op_val = op.read_u8_with_mode(&self, read_mode);
                let result = self.add8_update_flags(a.read_u8(&self), op_val, &[]);
                a.write_u8(&self, result);

                // TODO(PT): Cycle count should be 2 for (HL)
                InstrInfo::seq(1, 1)
            }
            "00ii1011" => {
                // DEC Reg16
                let dest = self.get_reg_from_lookup_tab2(i).0;
                if debug {
                    println!("DEC {dest}");
                }

                let prev = dest.read_u16(&self);
                let new = prev.overflowing_sub(1).0;

                dest.write_u16(&self, new);
                InstrInfo::seq(1, 2)
            }
            "10110iii" => {
                // OR A, Reg8
                let a = self.reg(RegisterName::A);
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("OR A, {op}");
                }
                let result = a.read_u8(&self) | op.read_u8_with_mode(&self, read_mode);
                a.write_u8(&self, result);
                self.set_flags(result == 0, false, false, false);
                // TODO(PT): Should be 2 for HL
                InstrInfo::seq(1, 1)
            }
            "10100iii" => {
                // AND A, Reg8
                // TODO(PT): Refactor with AND u8?
                let a = self.reg(RegisterName::A);
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                if debug {
                    println!("AND A, {op}");
                }
                let result = a.read_u8(&self) & op.read_u8_with_mode(&self, read_mode);
                a.write_u8(&self, result);
                self.set_flags(result == 0, false, true, false);
                // TODO(PT): Should be 2 for HL
                InstrInfo::seq(1, 1)
            }
            "110cf000" => {
                // RET FlagCondition
                // TODO(PT): Refactor this flag condition selection
                let flag_cond = match c {
                    // N/Z
                    0 => match f {
                        0 => FlagCondition::NotZero,
                        1 => FlagCondition::Zero,
                        _ => panic!("Invalid value"),
                    },
                    // N/C
                    1 => match f {
                        0 => FlagCondition::NotCarry,
                        1 => FlagCondition::Carry,
                        _ => panic!("Invalid value"),
                    },
                    _ => panic!("Invalid value"),
                };
                let should_return = self.is_flag_condition_met(flag_cond);
                if debug {
                    print!("RET if {flag_cond} ");
                    match should_return {
                        true => println!("(taken)"),
                        false => println!("(not taken)"),
                    }
                }
                if should_return {
                    self.set_pc(self.pop_u16());
                    InstrInfo::jump(1, 5)
                } else {
                    InstrInfo::seq(1, 2)
                }
            }
            "110cf010" => {
                // JP FlagCondition, u16
                let target = self.mmu.read_u16(self.get_pc() + 1);
                let flag_cond = match c {
                    // N/Z
                    0 => match f {
                        0 => FlagCondition::NotZero,
                        1 => FlagCondition::Zero,
                        _ => panic!("Invalid value"),
                    },
                    // N/C
                    1 => match f {
                        0 => FlagCondition::NotCarry,
                        1 => FlagCondition::Carry,
                        _ => panic!("Invalid value"),
                    },
                    _ => panic!("Invalid value"),
                };
                let should_jump = self.is_flag_condition_met(flag_cond);
                if debug {
                    print!("JP {target:04x} if {flag_cond} ");
                    match should_jump {
                        true => println!("(taken)"),
                        false => println!("(not taken)"),
                    }
                }
                let instr_size = 3;
                if should_jump {
                    self.set_pc(target);
                    InstrInfo::jump(instr_size, 4)
                } else {
                    InstrInfo::seq(instr_size, 3)
                }
            }
            "110cf100" => {
                // CALL FlagCondition, u16
                let target = self.mmu.read_u16(self.get_pc() + 1);
                let flag_cond = match c {
                    // N/Z
                    0 => match f {
                        0 => FlagCondition::NotZero,
                        1 => FlagCondition::Zero,
                        _ => panic!("Invalid value"),
                    },
                    // N/C
                    1 => match f {
                        0 => FlagCondition::NotCarry,
                        1 => FlagCondition::Carry,
                        _ => panic!("Invalid value"),
                    },
                    _ => panic!("Invalid value"),
                };
                let should_jump = self.is_flag_condition_met(flag_cond);
                if debug {
                    print!("CALL {target:04x} if {flag_cond} ");
                    match should_jump {
                        true => println!("(taken)"),
                        false => println!("(not taken)"),
                    }
                }
                let instr_size = 3;
                if should_jump {
                    self.call_addr(instr_size, target);
                    InstrInfo::jump(instr_size, 6)
                } else {
                    InstrInfo::seq(instr_size, 3)
                }
            }
            "11iii111" => {
                // RST Vector
                let target = (i as u16) * 8;
                if debug {
                    println!("RST 0x{target:04x}");
                }
                self.call_addr(1, target);
                InstrInfo::jump(1, 4)
            }
            "00ii1001" => {
                // ADD HL, Reg16
                let hl = self.reg(RegisterName::HL);
                let op = self.get_reg_from_lookup_tab2(i).0;

                if debug {
                    println!("ADD {hl}, {op}");
                }

                let hl_val = hl.read_u16(&self);
                let op_val = op.read_u16(&self);
                let result = self.add16_update_flags(hl_val, op_val, &[Flag::Zero]);
                hl.write_u16(&self, result);

                InstrInfo::seq(1, 2)
            }
            "10001iii" => {
                // ADC A, Reg8
                let a = self.reg(RegisterName::A);
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                let val = op.read_u8_with_mode(&self, read_mode);

                if debug {
                    println!("ADC {a}, {op}");
                }

                self.adc_a_u8(val);

                // TODO(PT): Should be 2 for HL
                InstrInfo::seq(1, 1)
            }
            "10111iii" => {
                // CP Reg8
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                let val = op.read_u8_with_mode(&self, read_mode);
                let a = self.reg(RegisterName::A);

                if debug {
                    println!("CP {op} with {a}");
                }

                self.instr_cp(val);
                // TODO(PT): Should be 2 for (HL)
                InstrInfo::seq(1, 1)
            }
            "10011iii" => {
                // SBC A, Reg8
                let a = self.reg(RegisterName::A);
                let (op, read_mode) = self.get_reg_from_lookup_tab1(i);
                let val = op.read_u8_with_mode(&self, read_mode);

                if debug {
                    println!("SBC {op} from {a}");
                }

                let result = self.sub8_with_carry_and_update_flags(a.read_u8(&self), val);
                a.write_u8(&self, result);

                // TODO(PT): Should be 2 for (HL)
                InstrInfo::seq(1, 1)
            }
            _ => {
                println!("<0x{:02x} is unimplemented>", instruction_byte);
                self.print_regs();
                //panic!("Unimplemented opcode");
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

    pub fn step(&mut self, system: &dyn GameBoyHardwareProvider) -> InstrInfo {
        let pc = self.get_pc();
        let info = self.decode(pc, system);
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

    pub fn set_halted(&mut self, halted: bool) {
        self.is_halted = halted
    }
}

#[cfg(test)]
mod tests {
    use std::{cell::RefCell, rc::Rc};

    use crate::{
        cpu::{AddressingMode, Flag, FlagCondition, FlagUpdate, RegisterName},
        gameboy::GameBoyHardwareProvider,
        interrupts::InterruptController,
        joypad::Joypad,
        mmu::{Mmu, Ram},
        ppu::Ppu,
    };

    use super::{CpuState, InstrInfo};

    // Notably, this is missing a PPU
    struct CpuTestSystem {
        pub mmu: Rc<Mmu>,
        pub cpu: Rc<RefCell<CpuState>>,
        interrupt_controller: Rc<InterruptController>,
    }

    impl CpuTestSystem {
        pub fn new(
            mmu: Rc<Mmu>,
            cpu: CpuState,
            interrupt_controller: Rc<InterruptController>,
        ) -> Self {
            Self {
                mmu,
                cpu: Rc::new(RefCell::new(cpu)),
                interrupt_controller,
            }
        }

        fn verify_instr_info(
            &self,
            instr_info: &InstrInfo,
            expected_instruction_size: u16,
            expected_cycle_count: usize,
        ) {
            assert_eq!(instr_info.instruction_size, expected_instruction_size);
            assert_eq!(instr_info.cycle_count, expected_cycle_count);
        }

        pub fn run_opcode_with_expected_attrs(
            &self,
            cpu: &mut CpuState,
            opcode: u8,
            expected_instruction_size: u16,
            expected_cycle_count: usize,
        ) {
            cpu.set_pc(0);
            self.mmu.write(0, opcode);
            let instr_info = cpu.step(self);
            self.verify_instr_info(&instr_info, expected_instruction_size, expected_cycle_count);
        }

        pub fn run_cb_opcode_with_expected_attrs(
            &self,
            cpu: &mut CpuState,
            opcode: u8,
            expected_cycle_count: usize,
        ) {
            cpu.set_pc(0);
            self.mmu.write(0, 0xcb);
            self.mmu.write(1, opcode);
            let instr_info = cpu.step(self);
            // All CB opcodes are 2 bytes in size
            self.verify_instr_info(&instr_info, 2, expected_cycle_count);
        }

        pub fn assert_flags(
            &self,
            cpu: &CpuState,
            zero: bool,
            subtract: bool,
            half_carry: bool,
            carry: bool,
        ) {
            assert_eq!(cpu.is_flag_set(Flag::Zero), zero);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), subtract);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), half_carry);
            assert_eq!(cpu.is_flag_set(Flag::Carry), carry);
        }
    }

    impl GameBoyHardwareProvider for CpuTestSystem {
        fn get_mmu(&self) -> Rc<Mmu> {
            Rc::clone(&self.mmu)
        }

        fn get_ppu(&self) -> Rc<Ppu> {
            panic!("PPU not supported in this test harness")
        }

        fn get_cpu(&self) -> Rc<RefCell<CpuState>> {
            Rc::clone(&self.cpu)
        }

        fn get_interrupt_controller(&self) -> Rc<crate::interrupts::InterruptController> {
            Rc::clone(&self.interrupt_controller)
        }

        fn get_joypad(&self) -> Rc<Joypad> {
            panic!("Joypad not supported in this test harness")
        }
    }

    fn get_system() -> CpuTestSystem {
        let ram = Rc::new(Ram::new(0, 0xffff));
        let mmu = Rc::new(Mmu::new(vec![ram]));
        let interrupt_controller = Rc::new(InterruptController::new());
        let mut cpu = CpuState::new(Rc::clone(&mmu));
        cpu.enable_debug();
        CpuTestSystem::new(mmu, cpu, interrupt_controller)
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

    #[test]
    fn test_flags_register_only_supports_high_bits() {
        // Given I write 0xff to the flags register
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::F).write_u8(&cpu, 0b11111111);
        // Then only the high bits are written
        assert_eq!(cpu.reg(RegisterName::F).read_u8(&cpu), 0b11110000);
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
            cpu.step(&gb);
            assert_eq!(cpu.reg(register).read_u8(&cpu), 4);
            assert_eq!(cpu.is_flag_set(Flag::Zero), false);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

            // Given the register contains 0 (underflow)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 0);
            cpu.step(&gb);
            assert_eq!(cpu.reg(register).read_u8(&cpu), 0xff);
            assert_eq!(cpu.is_flag_set(Flag::Zero), false);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);

            // Given the register contains 1 (zero)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 1);
            cpu.step(&gb);
            assert_eq!(cpu.reg(register).read_u8(&cpu), 0);
            assert_eq!(cpu.is_flag_set(Flag::Zero), true);
            assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
            assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

            // Given the register contains 0xf0 (half carry)
            cpu.set_pc(0);
            cpu.reg(register).write_u8(&cpu, 0xf0);
            cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
            cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);
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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
            cpu.step(&gb);
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

            cpu.step(&gb);

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
            cpu.step(&gb);
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
        cpu.step(&gb);

        // Then the Z flag is cleared
        assert!(cpu.is_flag_condition_met(FlagCondition::NotZero));

        // And when the pointee contains a value with its LSB unset
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x00);

        // When the CPU runs the instruction
        cpu.set_pc(0);
        cpu.mmu.write(0, 0xcb);
        cpu.mmu.write(1, 0x46);
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        cpu.step(&gb);

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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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

        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
        assert_eq!(instr_info.instruction_size, 1);
        //assert_eq!(instr_info.cycle_count, 2);
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
        let instr_info = cpu.step(&gb);
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
        let instr_info = cpu.step(&gb);
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
        cpu.step(&gb);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 4);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);

        // Given the register contains 0 (underflow)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0);
        cpu.step(&gb);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xff);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
        assert_eq!(cpu.is_flag_set(Flag::Carry), true);

        // Given the register contains 1 (zero)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 1);
        cpu.step(&gb);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0);
        assert_eq!(cpu.is_flag_set(Flag::Zero), true);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);

        // Given the register contains 0xf0 (half carry)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xf0);
        cpu.step(&gb);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xef);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
        assert_eq!(cpu.is_flag_set(Flag::Carry), false);
    }

    /* ADD A, Reg8 */

    #[test]
    fn test_add_a_reg8() {
        // Given an ADD A, Reg8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And A contains a value
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3a);
        // And B contains a value
        cpu.reg(RegisterName::B).write_u8(&cpu, 0xc6);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x80);
        cpu.step(&gb);

        // Then the result is stored in A
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        // And the flags are set correctly
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
    }

    /* DEC Reg16 */

    #[test]
    fn test_dec_reg16() {
        // Given a DEC SP instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And SP contains a value
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xffff);

        // When the CPU runs the instruction
        cpu.mmu.write(0, 0x3b);
        cpu.step(&gb);

        // Then SP has been decremented
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffe);
    }

    /* SWAP Reg8 */

    #[test]
    fn test_swap_reg8_deref_hl() {
        // Given a SWAP (HL) instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And (HL) contains a pointer
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        // And the pointee contains a value
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0xf0);

        // When the CPU runs the instruction
        gb.get_mmu().write(0, 0xcb);
        gb.get_mmu().write(1, 0x36);
        let instr_info = cpu.step(&gb);
        // TODO(PT): (HL) should take 4 cycles
        assert_eq!(instr_info.instruction_size, 2);
        assert_eq!(instr_info.cycle_count, 2);

        // Then the contents of (HL) have been swapped
        assert_eq!(gb.get_mmu().read(0x1234), 0x0f);

        // And the flags have been correctly updated
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    #[test]
    fn test_swap_reg8_a() {
        // Given a SWAP A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And A contains a value
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);

        // When the CPU runs the instruction
        gb.get_mmu().write(0, 0xcb);
        gb.get_mmu().write(1, 0x37);
        let instr_info = cpu.step(&gb);
        assert_eq!(instr_info.instruction_size, 2);
        assert_eq!(instr_info.cycle_count, 2);

        // Then the contents of A have been swapped
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);

        // And the flags have been correctly updated
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* DI */

    #[test]
    fn test_di() {
        // Given a DI instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And interrupts are enabled
        assert!(gb
            .get_interrupt_controller()
            .are_interrupts_globally_enabled());

        // When the CPU runs a DI instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xf3, 1, 1);

        // Then interrupts are disabled
        assert!(!gb
            .get_interrupt_controller()
            .are_interrupts_globally_enabled());
    }

    /* EI */

    #[test]
    fn test_ei() {
        // Given a EI instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And interrupts are disabled
        gb.get_interrupt_controller()
            .set_interrupts_globally_disabled();

        // When the CPU runs a EI instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xfb, 1, 1);

        // Then interrupts are enabled
        assert!(gb
            .get_interrupt_controller()
            .are_interrupts_globally_enabled());
    }

    /* OR A, Reg8 */

    #[test]
    fn test_or_reg8() {
        // Given an OR instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x5a);
        // And the Z flag is set
        cpu.set_flags(true, true, true, true);

        // When I run OR A, A
        // Then I get the expected result
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xb7, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x5a);
        assert!(!cpu.is_flag_set(Flag::Zero));
        // And the other flags are reset
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));

        // And when I run OR A, (HL)
        // TODO(PT): This variant should take 2 cycles
        cpu.set_flags(true, true, true, true);
        cpu.set_pc(0);
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0xffaa);
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x0f);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xb6, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x5f);
        assert!(!cpu.is_flag_set(Flag::Zero));
        // And the other flags are reset
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* OR A, u8 */

    #[test]
    fn test_or_u8() {
        // Given an OR instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x5a);
        cpu.set_flags(true, true, true, true);

        gb.get_mmu().write(1, 0x35);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xf6, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x7f);
        assert!(!cpu.is_flag_set(Flag::Zero));
        // And the other flags are reset
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));

        cpu.set_flags(true, true, true, true);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);
        gb.get_mmu().write(1, 0x00);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xf6, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Zero));
        // And the other flags are reset
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* AND A, Reg8 */

    #[test]
    fn test_and_reg() {
        // Given an AND instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x5a);
        cpu.reg(RegisterName::L).write_u8(&cpu, 0x3f);
        cpu.set_flags(true, true, true, true);

        // When I run AND A, L
        // Then I get the expected result
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xa5, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x1a);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));

        // And when I run AND A, (HL)
        cpu.set_pc(0);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x5a);
        cpu.reg(RegisterName::H).write_u8(&cpu, 0xff);
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x00);
        cpu.set_flags(false, false, false, false);
        // TODO(PT): This variant should take 2 cycles
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xa6, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* AND A, u8 */

    #[test]
    fn test_and_u8() {
        // Given an AND A, u8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.set_flags(true, true, true, true);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x5a);
        // And a value just after the instruction pointer
        gb.mmu.write(1, 0x38);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xe6, 2, 2);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x18);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* CPL A */

    #[test]
    fn test_cpl() {
        // Given a CPL instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x35);
        // Then the complement of A is computed
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x2f, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xca);
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
    }

    /* ADD A, u8 */

    #[test]
    fn test_add_u8() {
        // Given an ADD A, u8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And a value just after the instruction pointer
        gb.mmu.write(1, 0xff);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3c);

        gb.run_opcode_with_expected_attrs(&mut cpu, 0xc6, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x3b);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* SUB A, u8 */

    #[test]
    fn test_sub_u8() {
        // Given a SUB A, u8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // And a value just after the instruction pointer
        gb.mmu.write(1, 0x0f);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3e);

        gb.run_opcode_with_expected_attrs(&mut cpu, 0xd6, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x2f);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* CALL FlagCondition, u16 */

    #[test]
    fn test_call_flag_cond_u16() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let params = [
            // NZ Branch not taken
            (0xc4, FlagUpdate::Zero(true), false),
            // NZ Branch taken
            (0xc4, FlagUpdate::Zero(false), true),
            // Z Branch not taken
            (0xcc, FlagUpdate::Zero(false), false),
            // Z Branch taken
            (0xcc, FlagUpdate::Zero(true), true),
            // NC Branch not taken
            (0xd4, FlagUpdate::Carry(true), false),
            // NC Branch taken
            (0xd4, FlagUpdate::Carry(false), true),
            // C Branch not taken
            (0xdc, FlagUpdate::Carry(false), false),
            // C Branch taken
            (0xdc, FlagUpdate::Carry(true), true),
        ];
        for (opcode, given_flag, expected_jump) in params {
            cpu.set_pc(0);
            // And there is a stack set up
            cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffe);
            // And there is a jump target just after the opcode
            gb.mmu.write_u16(1, 0x4455);

            let expected_cycles = if expected_jump { 6 } else { 3 };
            cpu.set_flags(false, false, false, false);
            cpu.update_flag(given_flag);
            gb.run_opcode_with_expected_attrs(&mut cpu, opcode, 3, expected_cycles);

            if expected_jump {
                // Then PC has been redirected to the jump target
                assert_eq!(cpu.get_pc(), 0x4455);

                // And the stack pointer has been decremented due to the return address
                // stored on the stack
                assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffc);

                // And the return address is stored on the stack
                assert_eq!(gb.mmu.read_u16(cpu.reg(RegisterName::SP).read_u16(&cpu)), 3);
            } else {
                // Then PC has not been redirected
                assert_eq!(cpu.get_pc(), 3);
                // And the stack is left untouched
                assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffe);
            }
        }
    }

    /* SRL Reg8 */

    #[test]
    fn test_srl() {
        // Given a SRL A instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.set_flags(true, true, true, false);

        cpu.reg(RegisterName::A).write_u8(&cpu, 0b10011001);
        gb.mmu.write(0, 0xcb);
        gb.mmu.write(1, 0x3f);
        // TODO(PT): Should be 4 cycles for (HL)
        let instr_info = cpu.step(&gb);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0b01001100);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* RR Reg8 */

    #[test]
    fn test_rr() {
        // Given a RR B instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.set_flags(false, false, false, false);

        // And the Carry flag is set
        cpu.update_flag(FlagUpdate::Carry(true));
        cpu.reg(RegisterName::B).write_u8(&cpu, 0b10011000);

        // When the CPU runs the instruction
        gb.mmu.write(0, 0xcb);
        gb.mmu.write(1, 0x18);
        // TODO(PT): Should be 4 cycles for (HL)
        let instr_info = cpu.step(&gb);

        assert_eq!(cpu.reg(RegisterName::B).read_u8(&cpu), 0b11001100);
        // And the LSB has been copied to the Carry flag
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
    }

    #[test]
    fn test_rra() {
        // Given a RRA instruction (in the single-byte-opcode variant)
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        // And the Carry flag is not set
        cpu.set_flags(false, false, false, false);

        cpu.reg(RegisterName::A).write_u8(&cpu, 0b10011001);

        // When the CPU runs the instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x1f, 1, 1);

        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0b01001100);
        // And the LSB has been copied to the Carry flag
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
    }

    /* ADD HL, Reg16 */

    #[test]
    fn test_add_hl_reg16() {
        // Given an ADD HL, SP instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.set_flags(false, false, false, false);

        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x8a23);
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0x0605);

        // When the CPU runs the instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x39, 1, 2);

        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0x9028);
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));

        // When the CPU runs the instruction
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x8a23);
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0x8a23);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x39, 1, 2);

        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0x1446);
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* ADD SP, i8 */

    #[test]
    fn test_add_sp_i8() {
        // Given an ADD SP, i8 instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.set_flags(false, false, false, false);

        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfff8);

        // When the CPU runs the instruction
        gb.mmu.write(1, 0x02);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xe8, 2, 4);
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffa);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));

        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfff8);
        gb.mmu.write(1, -2_i8 as u8);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xe8, 2, 4);
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfff6);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* RES Bit, Reg8 */

    #[test]
    fn test_res_bit_reg8() {
        // Given a RES 5, B instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::B).write_u8(&cpu, 0b10100001);
        // When the CPU runs the instruction
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0xa8, 2);
        // Then the 5th bit has been reset
        assert_eq!(cpu.reg(RegisterName::B).read_u8(&cpu), 0b10000001);
    }

    /* SLA Reg8 */

    #[test]
    fn test_sla_reg8() {
        // Given a SLA L instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::L).write_u8(&cpu, 0b10010011);
        // When the CPU runs the instruction
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x25, 2);

        // Then the high bit has been copied to the CY flag
        assert!(cpu.is_flag_set(Flag::Carry));
        // And the register has been left-shifted
        assert_eq!(cpu.reg(RegisterName::L).read_u8(&cpu), 0b00100110);
    }

    /* ADC A, u8 | ADC A, Reg8 */

    #[test]
    fn test_adc_a() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0xe1);
        cpu.reg(RegisterName::E).write_u8(&cpu, 0x0f);
        // Make sure HL points somewhere valid
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        cpu.reg(RegisterName::HL)
            .write_u8_with_mode(&cpu, AddressingMode::Deref, 0x1e);
        cpu.set_flags(false, false, false, true);

        gb.run_opcode_with_expected_attrs(&mut cpu, 0x8b, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xf1);
        gb.assert_flags(&cpu, false, false, true, false);

        // Reset state
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xe1);
        cpu.set_flags(false, false, false, true);

        gb.get_mmu().write(1, 0x3b);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xce, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x1d);
        gb.assert_flags(&cpu, false, false, false, true);

        // Reset state
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xe1);
        cpu.set_flags(false, false, false, true);

        // TODO(PT): Should take 2 cycles
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x8e, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        gb.assert_flags(&cpu, true, false, true, true);
    }

    /* SET Bit, Reg8 */

    #[test]
    fn test_set_bit_reg8() {
        // Given a SET 5, L instruction
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        cpu.reg(RegisterName::L).write_u8(&cpu, 0b10000001);
        // When the CPU runs the instruction
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0xed, 2);
        // Then the 5th bit has been set
        assert_eq!(cpu.reg(RegisterName::L).read_u8(&cpu), 0b10100001);
    }

    /* RETI */

    #[test]
    fn test_reti() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // Given interrupts are disabled, because we're in an interrupt handler
        let int_controller = gb.get_interrupt_controller();
        int_controller.set_interrupts_globally_disabled();

        // And there is a stack containing a return address
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xff00);
        gb.get_mmu().write_u16(0xff00, 0x1234);

        // When the CPU runs a RETI instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xd9, 1, 4);

        // Then PC has been set to the address stored on the stack
        assert_eq!(cpu.get_pc(), 0x1234);
        // And the stack pointer has been incremented
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xff02);
        // And interrupts are re-enabled
        assert!(int_controller.are_interrupts_globally_enabled());
    }

    /* SRA Reg8 */

    #[test]
    fn test_sra_reg8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x8a);
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x2f, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xc5);

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x01);
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x2f, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Carry));

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x2f, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Zero));
    }

    /* RRC Reg8 */

    #[test]
    fn test_rrc_reg8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x01);
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x0f, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x80);
        assert!(cpu.is_flag_set(Flag::Carry));

        // Reset flags
        cpu.set_flags(false, false, false, false);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x00);
        gb.run_cb_opcode_with_expected_attrs(&mut cpu, 0x2f, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Zero));

        // Try the non-cb variant
        cpu.set_flags(false, false, false, false);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0b01110001);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x0f, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0b10111000);
        assert!(cpu.is_flag_set(Flag::Carry));
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    /* JP HL */

    #[test]
    fn test_jp_hl() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xe9, 1, 1);
        assert_eq!(cpu.get_pc(), 0x1234);
    }

    /* XOR Reg8 */

    #[test]
    fn test_xor_reg8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // When I XOR A with itself
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xff);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xaf, 1, 1);
        // Then A is set to zero
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        // And the Z flag is set
        assert!(cpu.is_flag_set(Flag::Zero));

        // When I XOR A with (HL)
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xff);
        // Make sure HL points somewhere valid
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        cpu.reg(RegisterName::HL).write_u8(&cpu, 0x8a);
        // TODO(PT): Should take 2 cycles
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xae, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x75);
        assert!(!cpu.is_flag_set(Flag::Zero));

        // When I XOR A with 0x0f
        cpu.reg(RegisterName::A).write_u8(&cpu, 0xff);
        gb.get_mmu().write(1, 0x0f);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xee, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xf0);
        assert!(!cpu.is_flag_set(Flag::Zero));
    }

    /* LD (u16), SP */

    #[test]
    fn test_load_u16_sp() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // Given the stack pointer contains an address
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0x1234);
        gb.get_mmu().write_u16(1, 0x1000);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x08, 3, 5);

        // Then the stack pointer has been stored at the pointee
        assert_eq!(gb.get_mmu().read_u16(0x1000), 0x1234);
    }

    /* LD SP, HL */

    #[test]
    fn test_load_sp_with_hl() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // Given HL and SP contain addresses
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0x5555);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xf9, 1, 2);

        // Then the value of HL is stored in SP
        assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0x1234);
    }

    /* SCF */

    #[test]
    fn test_scf() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.set_flags(false, true, true, false);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x37, 1, 1);
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* CCF */

    #[test]
    fn test_ccf() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.set_flags(true, true, true, true);

        // Given the carry flag is false
        cpu.update_flag(FlagUpdate::Carry(false));
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x3f, 1, 1);
        // Then the flag is flipped to true
        assert!(cpu.is_flag_set(Flag::Carry));
        // And the N and H flags are unset
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));

        // Given the carry flag is true
        cpu.update_flag(FlagUpdate::Carry(true));
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x3f, 1, 1);
        // Then the flag is flipped to false
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* SBC A, Reg8 | SBC A, u8 */

    #[test]
    fn test_sbc_reg8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // SBC A, H
        cpu.set_flags(false, false, false, true);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3b);
        cpu.reg(RegisterName::H).write_u8(&cpu, 0x2a);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x9c, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x10);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));

        // SBC A, 0x3a
        cpu.set_flags(false, false, false, true);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3b);
        gb.get_mmu().write(1, 0x3a);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xde, 2, 2);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x00);
        assert!(cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));

        // SBC A, (HL)
        cpu.set_flags(false, false, false, true);
        cpu.reg(RegisterName::A).write_u8(&cpu, 0x3b);
        cpu.reg(RegisterName::HL).write_u16(&cpu, 0x1234);
        cpu.reg(RegisterName::HL).write_u8(&cpu, 0x4f);
        // TODO(PT): Should be 2 cycles for (HL)
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x9e, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0xeb);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(cpu.is_flag_set(Flag::HalfCarry));
        assert!(cpu.is_flag_set(Flag::Subtract));
        assert!(cpu.is_flag_set(Flag::Carry));
    }

    /* LD HL, SP+i8 */

    #[test]
    fn test_ld_hl_sp_i8() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.set_flags(true, true, true, true);
        cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfff8);
        gb.get_mmu().write(1, 0x02);
        gb.run_opcode_with_expected_attrs(&mut cpu, 0xf8, 2, 3);
        assert_eq!(cpu.reg(RegisterName::HL).read_u16(&cpu), 0xfffa);
        assert!(!cpu.is_flag_set(Flag::Zero));
        assert!(!cpu.is_flag_set(Flag::HalfCarry));
        assert!(!cpu.is_flag_set(Flag::Subtract));
        assert!(!cpu.is_flag_set(Flag::Carry));
    }

    /* DAA */

    #[test]
    fn test_daa() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x45);
        cpu.reg(RegisterName::B).write_u8(&cpu, 0x38);
        // Add A and B
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x80, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x7d);
        assert!(!cpu.is_flag_set(Flag::Subtract));
        // Run DAA
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x27, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x83);
        assert!(!cpu.is_flag_set(Flag::Carry));

        // Sub A and B
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x90, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x4b);
        assert!(cpu.is_flag_set(Flag::Subtract));
        // Run DAA
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x27, 1, 1);
        assert_eq!(cpu.reg(RegisterName::A).read_u8(&cpu), 0x45);
    }

    /* RST Vector */

    #[test]
    fn test_rst() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        cpu.reg(RegisterName::A).write_u8(&cpu, 0x45);
        cpu.reg(RegisterName::B).write_u8(&cpu, 0x38);

        for reset_vector in 0..8 {
            let reset_vector_address = reset_vector * 8;

            // Set up a stack pointer
            cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffc);
            // Set up a current PC
            cpu.reg(RegisterName::PC).write_u16(&cpu, 0x1234);

            let opcode = 0b11000111 | ((reset_vector as u8) << 3);
            gb.get_mmu().write(0x1234, opcode);
            // When the CPU runs a RST instruction
            let instr_info = cpu.step(&gb);
            assert_eq!(instr_info.instruction_size, 1);
            assert_eq!(instr_info.cycle_count, 4);

            // Then the address of the next instruction after the RST
            // has been pushed to the stack
            assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffa);
            assert_eq!(gb.get_mmu().read_u16(0xfffa), 0x1235);
            // And the PC has been redirected to the reset vector
            assert_eq!(
                cpu.reg(RegisterName::PC).read_u16(&cpu),
                reset_vector_address
            );
        }
    }

    /* RET FlagCond */

    #[test]
    fn test_ret_flag_cond() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let params = [
            // NZ Branch not taken
            (0xc0, FlagUpdate::Zero(true), false),
            // NZ Branch taken
            (0xc0, FlagUpdate::Zero(false), true),
            // Z Branch not taken
            (0xc8, FlagUpdate::Zero(false), false),
            // Z Branch taken
            (0xc8, FlagUpdate::Zero(true), true),
            // NC Branch not taken
            (0xd0, FlagUpdate::Carry(true), false),
            // NC Branch taken
            (0xd0, FlagUpdate::Carry(false), true),
            // C Branch not taken
            (0xd8, FlagUpdate::Carry(false), false),
            // C Branch taken
            (0xd8, FlagUpdate::Carry(true), true),
        ];
        for (opcode, given_flag, expected_jump) in params {
            cpu.set_pc(0);
            // And there is a stack set up
            cpu.reg(RegisterName::SP).write_u16(&cpu, 0xfffa);
            // And the stack contains a return address
            gb.get_mmu().write_u16(0xfffa, 0x1234);

            let expected_cycles = if expected_jump { 5 } else { 2 };
            cpu.set_flags(false, false, false, false);
            cpu.update_flag(given_flag);
            gb.run_opcode_with_expected_attrs(&mut cpu, opcode, 1, expected_cycles);

            if expected_jump {
                // Then PC has been redirected to the return address stored on the stack
                assert_eq!(cpu.get_pc(), 0x1234);

                // And the stack pointer has been incremented due to the return address
                // popped off the stack
                assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffc);
            } else {
                // Then PC has not been redirected
                assert_eq!(cpu.get_pc(), 1);
                // And the stack is left untouched
                assert_eq!(cpu.reg(RegisterName::SP).read_u16(&cpu), 0xfffa);
            }
        }
    }

    /* JP FlagCond, u16 */

    #[test]
    fn test_jp_flag_cond_u16() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();
        let params = [
            // NZ Branch not taken
            (0xc2, FlagUpdate::Zero(true), false),
            // NZ Branch taken
            (0xc2, FlagUpdate::Zero(false), true),
            // Z Branch not taken
            (0xca, FlagUpdate::Zero(false), false),
            // Z Branch taken
            (0xca, FlagUpdate::Zero(true), true),
            // NC Branch not taken
            (0xd2, FlagUpdate::Carry(true), false),
            // NC Branch taken
            (0xd2, FlagUpdate::Carry(false), true),
            // C Branch not taken
            (0xda, FlagUpdate::Carry(false), false),
            // C Branch taken
            (0xda, FlagUpdate::Carry(true), true),
        ];
        for (opcode, given_flag, expected_jump) in params {
            cpu.set_pc(0);
            // Given there is a jump target just after the opcode
            gb.get_mmu().write_u16(1, 0x1234);

            let expected_cycles = if expected_jump { 4 } else { 3 };
            cpu.set_flags(false, false, false, false);
            cpu.update_flag(given_flag);
            gb.run_opcode_with_expected_attrs(&mut cpu, opcode, 3, expected_cycles);

            if expected_jump {
                // Then PC has been redirected to the immediate jump target
                assert_eq!(cpu.get_pc(), 0x1234);
            } else {
                // Then PC has not been redirected
                assert_eq!(cpu.get_pc(), 3);
            }
        }
    }

    /* HALT */

    #[test]
    fn test_halt() {
        let gb = get_system();
        let mut cpu = gb.cpu.borrow_mut();

        // When the CPU runs a HALT instruction
        gb.run_opcode_with_expected_attrs(&mut cpu, 0x76, 1, 4);

        // Then the CPU is now halted
        assert!(cpu.is_halted);
    }
}
