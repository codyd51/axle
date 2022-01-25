use core::mem;
use std::{
    cell::RefCell,
    collections::BTreeMap,
    env::VarError,
    fmt::{Debug, Display},
};

use alloc::vec::Vec;

use bitmatch::bitmatch;

trait ReadSize {
    const BYTE_COUNT: usize;
    fn from(val: usize) -> Self;
    fn shr(val: Self, rhs: usize) -> Self;
    fn as_u8(val: usize) -> u8 {
        val.try_into().unwrap()
    }
}

impl ReadSize for u8 {
    const BYTE_COUNT: usize = mem::size_of::<u8>();
    fn from(val: usize) -> Self {
        val.try_into().expect("u8 overflow")
    }

    fn shr(val: Self, rhs: usize) -> u8 {
        (val >> rhs).try_into().unwrap()
    }
}

impl ReadSize for i8 {
    const BYTE_COUNT: usize = mem::size_of::<i8>();
    fn from(val: usize) -> Self {
        val as i8
    }

    fn shr(val: Self, rhs: usize) -> i8 {
        (val >> rhs).try_into().unwrap()
    }
}

impl ReadSize for u16 {
    const BYTE_COUNT: usize = mem::size_of::<u16>();
    fn from(val: usize) -> Self {
        val.try_into().expect("u16 overflow")
    }

    fn shr(val: Self, rhs: usize) -> u16 {
        (val >> rhs).try_into().unwrap()
    }
}

struct Memory {
    rom: RefCell<[u8; 0xffff]>,
}

impl Memory {
    fn new() -> Self {
        Self {
            rom: RefCell::new([0; 0xffff]),
        }
    }

    fn read<T: ReadSize>(&self, offset: u16) -> T {
        let mut val = 0usize;
        let rom = self.rom.borrow();
        for i in 0..T::BYTE_COUNT {
            let byte = rom[(offset as usize) + i];
            val |= (byte as usize) << (i * 8);
        }
        T::from(val)
    }

    // TODO(PT): Refactor this with a generic WriteSize like read()?
    fn write_u8(&self, offset: u16, value: u8) {
        let mut rom = self.rom.borrow_mut();
        rom[offset as usize] = value;
    }

    fn write_u16(&self, offset: u16, value: u16) {
        let mut rom = self.rom.borrow_mut();
        rom[offset as usize] = value as u8;
        rom[(offset + 1) as usize] = (value >> 8) as u8;
    }
}

struct InstrInfo {
    instruction_size: u16,
    cycle_count: usize,
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

pub struct CpuState {
    flags: RefCell<u8>,
    operands: BTreeMap<OperandName, Box<dyn VariableStorage>>,
    memory: Memory,
    debug_enabled: bool,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum OperandName {
    // 8-bit operands
    RegB,
    RegC,
    RegD,
    RegE,
    RegH,
    RegL,
    RegA,

    // 16-bit operands
    RegsBC,
    RegsDE,
    RegsHL,

    RegSP,
    RegPC,
}

impl Display for OperandName {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let name = match self {
            OperandName::RegB => "B",
            OperandName::RegC => "C",
            OperandName::RegD => "D",
            OperandName::RegE => "E",
            OperandName::RegH => "H",
            OperandName::RegL => "L",
            OperandName::RegA => "A",

            OperandName::RegsBC => "BC",
            OperandName::RegsDE => "DE",
            OperandName::RegsHL => "HL",

            OperandName::RegSP => "SP",
            OperandName::RegPC => "PC",
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
        write!(f, "[Reg {} ({:02x})]", self.name, *self.contents.borrow())
    }
}

#[derive(Debug)]
struct CpuRegisterPair {
    upper: OperandName,
    lower: OperandName,
}

impl CpuRegisterPair {
    fn new(upper: OperandName, lower: OperandName) -> Self {
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
            AddressingMode::Deref => cpu.memory.read(address),
            AddressingMode::DerefThenIncrement => {
                self.write_u16(cpu, address + 1);
                cpu.memory.read(address)
            }
            AddressingMode::DerefThenDecrement => {
                self.write_u16(cpu, address - 1);
                cpu.memory.read(address)
            }
        }
    }

    fn read_u16(&self, cpu: &CpuState) -> u16 {
        let upper = cpu.get_op(self.upper);
        let lower = cpu.get_op(self.lower);
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
            AddressingMode::Deref => cpu.memory.write_u8(address, val),
            AddressingMode::DerefThenIncrement => {
                self.write_u16(cpu, address + 1);
                cpu.memory.write_u8(address, val)
            }
            AddressingMode::DerefThenDecrement => {
                self.write_u16(cpu, address - 1);
                cpu.memory.write_u8(address, val)
            }
        }
    }

    fn write_u16(&self, cpu: &CpuState, val: u16) {
        let upper = cpu.get_op(self.upper);
        let lower = cpu.get_op(self.lower);

        upper.write_u8(cpu, ((val >> 8) & 0xff).try_into().unwrap());
        lower.write_u8(cpu, (val & 0xff).try_into().unwrap());
    }
}

impl Display for CpuRegisterPair {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "[RegPair {}{}]", self.upper, self.lower)
    }
}

#[derive(Debug)]
struct CpuWideRegister {
    display_name: String,
    name: OperandName,
    contents: RefCell<u16>,
}

impl CpuWideRegister {
    fn new(name: OperandName) -> Self {
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
            AddressingMode::Read => "Read",
            AddressingMode::Deref => "Deref",
            AddressingMode::DerefThenIncrement => "Deref+",
            AddressingMode::DerefThenDecrement => "Deref-",
        };
        write!(f, "{}", name)
    }
}

// OpInfo { OpName, Op, DerefMode }

impl CpuState {
    pub fn new() -> Self {
        let mut operands: BTreeMap<OperandName, Box<dyn VariableStorage>> = BTreeMap::new();

        // 8-bit operands
        operands.insert(OperandName::RegB, Box::new(CpuRegister::new("B")));
        operands.insert(OperandName::RegC, Box::new(CpuRegister::new("C")));
        operands.insert(OperandName::RegD, Box::new(CpuRegister::new("D")));
        operands.insert(OperandName::RegE, Box::new(CpuRegister::new("E")));
        operands.insert(OperandName::RegH, Box::new(CpuRegister::new("H")));
        operands.insert(OperandName::RegL, Box::new(CpuRegister::new("L")));
        operands.insert(OperandName::RegA, Box::new(CpuRegister::new("A")));

        // 16-bit operands
        operands.insert(
            OperandName::RegsBC,
            Box::new(CpuRegisterPair::new(OperandName::RegB, OperandName::RegC)),
        );
        operands.insert(
            OperandName::RegsDE,
            Box::new(CpuRegisterPair::new(OperandName::RegD, OperandName::RegE)),
        );
        operands.insert(
            OperandName::RegsHL,
            Box::new(CpuRegisterPair::new(OperandName::RegH, OperandName::RegL)),
        );
        // Stack pointer
        operands.insert(
            OperandName::RegSP,
            Box::new(CpuWideRegister::new(OperandName::RegSP)),
        );
        // Program counter
        operands.insert(
            OperandName::RegPC,
            Box::new(CpuWideRegister::new(OperandName::RegPC)),
        );

        Self {
            flags: RefCell::new(0),
            operands,
            memory: Memory::new(),
            debug_enabled: false,
        }
    }

    pub fn load_rom_data(&mut self, rom_data: Vec<u8>) {
        // Set initial state
        self.set_flags(true, false, true, true);
        self.get_op(OperandName::RegPC).write_u16(self, 0x0100);
        self.get_op(OperandName::RegSP).write_u16(self, 0xfffe);

        self.get_op(OperandName::RegA).write_u8(self, 0x01);
        self.get_op(OperandName::RegC).write_u8(self, 0x13);
        self.get_op(OperandName::RegE).write_u8(self, 0xd8);
        self.get_op(OperandName::RegH).write_u8(self, 0x01);
        self.get_op(OperandName::RegL).write_u8(self, 0x4d);

        let mut rom = self.memory.rom.borrow_mut();
        rom[..rom_data.len()].copy_from_slice(&rom_data);
    }

    pub fn get_pc(&self) -> u16 {
        // TODO(PT): Rename Operand to Register?
        self.get_op(OperandName::RegPC).read_u16(self)
    }

    pub fn set_pc(&self, val: u16) {
        self.get_op(OperandName::RegPC).write_u16(self, val)
    }

    pub fn enable_debug(&mut self) {
        self.debug_enabled = true;
    }

    pub fn print_regs(&self) {
        println!();
        println!("--- CPU State ---");
        //println!("\tPC = 0x{:04x}\tSP = 0x{:04x}", self.pc, self.sp);
        //println!("\tPC = 0x{:04x}\tSP = 0x{:04x}", self.pc, self.sp);
        let flags = self.format_flags();
        println!(
            "\t{}\t{}",
            self.get_op(OperandName::RegPC),
            self.get_op(OperandName::RegSP)
        );
        println!("\tFlags: {flags}");

        for (name, operand) in &self.operands {
            // Some registers are handled above
            if ![OperandName::RegPC, OperandName::RegSP].contains(name) {
                println!("\t{name}: {operand}");
            }
        }
    }

    fn set_flags(&mut self, z: bool, n: bool, h: bool, c: bool) {
        let high_nibble = c as u8 | ((h as u8) << 1) | ((n as u8) << 2) | ((z as u8) << 3);
        let mut flags = self.flags.borrow_mut();
        *flags = high_nibble << 4;
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

        let mut flags = self.flags.borrow_mut();
        if flag_setting_and_bit_index.0 {
            // Enable flag
            *flags |= 1 << bit_index;
        } else {
            // Disable flag
            *flags &= !(1 << bit_index);
        }
    }

    fn is_flag_set(&self, flag: Flag) -> bool {
        let mut flag_bit_index = match flag {
            Flag::Zero => 3,
            Flag::Subtract => 2,
            Flag::HalfCarry => 1,
            Flag::Carry => 0,
        };
        let flags = *self.flags.borrow();
        (flags & (1 << (4 + flag_bit_index))) != 0
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

    fn get_op_name_and_addressing_mode_from_lookup_tab1(
        &self,
        index: u8,
    ) -> (OperandName, AddressingMode) {
        match index {
            0 => (OperandName::RegB, AddressingMode::Read),
            1 => (OperandName::RegC, AddressingMode::Read),
            2 => (OperandName::RegD, AddressingMode::Read),
            3 => (OperandName::RegE, AddressingMode::Read),
            4 => (OperandName::RegH, AddressingMode::Read),
            5 => (OperandName::RegL, AddressingMode::Read),
            7 => (OperandName::RegA, AddressingMode::Read),

            6 => (OperandName::RegsHL, AddressingMode::Deref),

            _ => panic!("Invalid index"),
        }
    }

    fn get_op_name_and_addressing_mode_from_lookup_tab2(
        &self,
        index: u8,
    ) -> (OperandName, AddressingMode) {
        match index {
            0 => (OperandName::RegsBC, AddressingMode::Read),
            1 => (OperandName::RegsDE, AddressingMode::Read),
            2 => (OperandName::RegsHL, AddressingMode::Read),
            3 => (OperandName::RegSP, AddressingMode::Read),
            _ => panic!("Invalid index"),
        }
    }

    fn get_op_name_and_addressing_mode_from_lookup_tab3(
        &self,
        index: u8,
    ) -> (OperandName, AddressingMode) {
        match index {
            0 => (OperandName::RegsBC, AddressingMode::Deref),
            1 => (OperandName::RegsDE, AddressingMode::Deref),
            2 => (OperandName::RegsHL, AddressingMode::DerefThenIncrement),
            3 => (OperandName::RegsHL, AddressingMode::DerefThenDecrement),
            _ => panic!("Invalid index"),
        }
    }

    fn get_op_from_lookup_tab1(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_op_name_and_addressing_mode_from_lookup_tab1(index);
        (self.get_op(name), mode)
    }

    fn get_op_from_lookup_tab2(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_op_name_and_addressing_mode_from_lookup_tab2(index);
        (self.get_op(name), mode)
    }

    fn get_op_from_lookup_tab3(&self, index: u8) -> (&dyn VariableStorage, AddressingMode) {
        let (name, mode) = self.get_op_name_and_addressing_mode_from_lookup_tab3(index);
        (self.get_op(name), mode)
    }

    pub fn get_op(&self, name: OperandName) -> &dyn VariableStorage {
        &*self.operands[&name]
    }

    #[bitmatch]
    fn decode(&mut self, pc: u16) -> InstrInfo {
        // Fetch the next opcode
        let instruction_byte: u8 = self.memory.read(pc);
        // Decode using the strategy described by https://gb-archive.github.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html
        let opcode_digit1 = (instruction_byte >> 6) & 0b11;
        let opcode_digit2 = (instruction_byte >> 3) & 0b111;
        let opcode_digit3 = (instruction_byte >> 0) & 0b111;

        let debug = self.debug_enabled;
        if debug {
            print!("0x{:04x}\t{:02x}\t", self.get_pc(), instruction_byte);
            print!(
                "{:02x} {:02x} {:02x}\t",
                opcode_digit1, opcode_digit2, opcode_digit3
            );
        }

        // Some instructions have dedicated handling
        let maybe_instr_info = match instruction_byte {
            0x00 => {
                if debug {
                    println!("NOP");
                }
                Some(InstrInfo::seq(1, 1))
            }
            0x76 => {
                todo!("HALT")
            }
            0xc3 => {
                let target = self.memory.read(self.get_pc() + 1);
                if debug {
                    println!("JMP 0x{target:04x}");
                }
                self.get_op(OperandName::RegPC).write_u16(self, target);
                Some(InstrInfo::jump(3, 4))
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
            "00iii101" => {
                // DEC [Reg]
                let (op, read_mode) = self.get_op_from_lookup_tab1(i);
                if debug {
                    print!("DEC {op}\t");
                }
                let prev = op.read_u8_with_mode(&self, read_mode);
                let new = prev - 1;
                op.write_u8(&self, new);
                self.update_flag(FlagUpdate::Zero(new == 0));
                self.update_flag(FlagUpdate::Subtract(true));
                // Underflow into the high nibble?
                self.update_flag(FlagUpdate::HalfCarry((prev & 0xf) < (new & 0xf)));

                if debug {
                    println!("Result: {op}")
                }
                InstrInfo::seq(1, 1)
                // TODO(PT): The commented expression is for half-carry addition
                //half_carry_flag = (((prev & 0xf) + (new_value & 0xf)) & 0x10) == 0x10;
            }
            "01tttfff" => {
                // Opcode is 0x40 to 0x7f
                // LD [Reg], [Reg]
                let (from_op, from_op_read_mode) = self.get_op_from_lookup_tab1(f);
                let from_val = from_op.read_u8_with_mode(self, from_op_read_mode);
                let to_op = self.get_op_from_lookup_tab1(t).0;

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
                    self.get_op_name_and_addressing_mode_from_lookup_tab1(f).0,
                    self.get_op_name_and_addressing_mode_from_lookup_tab1(f).0,
                ];
                let cycle_count = if operand_names.contains(&OperandName::RegsHL) {
                    2
                } else {
                    1
                };
                InstrInfo::seq(1, cycle_count)
            }
            "00iii110" => {
                // LD [Reg], [u8]
                let dest = self.get_op_from_lookup_tab1(i).0;
                let val = self.memory.read(self.get_pc() + 1);
                dest.write_u8(&self, val);
                if debug {
                    println!("LD {dest} with 0x{val:02x}");
                }
                // TODO(PT): Update me when the operand is (HL)
                InstrInfo::seq(2, 2)
            }
            "10101iii" => {
                // XOR [Reg]
                let (operand, read_mode) = self.get_op_from_lookup_tab1(i);
                let reg_a = self.get_op(OperandName::RegA);

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
                let dest = self.get_op_from_lookup_tab2(i).0;
                let val = self.memory.read(self.get_pc() + 1);

                if debug {
                    println!("LD {dest} with 0x{val:02x}");
                }

                dest.write_u16(&self, val);
                InstrInfo::seq(3, 3)
            }
            "00ii0010" => {
                // LD (Reg16), A
                let src = self.get_op(OperandName::RegA);
                let (dest, dest_addressing_mode) = self.get_op_from_lookup_tab3(i);

                if debug {
                    println!("LOAD {dest} {dest_addressing_mode} with {src}");
                }

                dest.write_u8_with_mode(&self, dest_addressing_mode, src.read_u8(&self));

                InstrInfo::seq(1, 2)
            }
            "00ii1010" => {
                // LD A, [MemOp]
                let dest = self.get_op(OperandName::RegA);
                let (src, src_addressing_mode) = self.get_op_from_lookup_tab3(i);

                if debug {
                    println!("LOAD {dest} with {src} {src_addressing_mode}");
                }

                dest.write_u8(&self, src.read_u8_with_mode(&self, src_addressing_mode));

                InstrInfo::seq(1, 2)
            }
            _ => {
                println!("<0x{:02x} is unimplemented>", instruction_byte);
                panic!("Unimplemented opcode")
            }
        }

        /*
        } else {
            match instruction_byte {
                /*
                }
                0x02 => {
                    let bc = self.get_bc();
                    let mem = self.memory.read(bc);
                    self.registers.a = mem;
                    if debug {
                        println!("A = [BC] ({bc:04x}: {mem:02x})");
                    }
                    InstrInfo::seq(1, 2)
                }
                0x0c => {
                    self.registers.c += 1;
                    if debug {
                        println!("C += 1");
                    }
                    InstrInfo::seq(1, 1)
                }
                0x20 => {
                    if !self.is_flag_set(Flag::Zero) {
                        let rel_target: i8 = self.memory.read(self.pc + 1);
                        if debug {
                            println!("JR NZ +{:02x};\t(taken)", rel_target);
                        }
                        // Add 2 to pc before doing the relative target, as
                        // this instruction is 2 bytes wide
                        self.pc += 2;
                        self.pc += rel_target as u16;
                        InstrInfo::jump(2, 3)
                    } else {
                        if debug {
                            println!("JR NZ +off;\t(not taken)");
                        }
                        InstrInfo::seq(2, 2)
                    }
                }
                0x21 => {
                    self.set_hl(self.memory.read(self.pc + 1));
                    if debug {
                        println!("HL = 0x{:04x}", self.get_hl());
                    }
                    InstrInfo::seq(3, 3)
                }
                0x32 => {
                    let hl = self.get_hl();
                    self.memory.write_u8(hl, self.get_a());
                    self.set_hl(hl - 1);
                    if debug {
                        println!("LD (HL-) = A;\t*0x{:04x} = 0x{:02x}", hl, self.get_a());
                    }
                    InstrInfo::seq(1, 2)
                }
                0xaf => {
                    self.registers.a ^= self.registers.a;
                    if debug {
                        println!("A ^= A");
                    }
                    self.set_flags(true, false, false, false);
                    InstrInfo::seq(1, 1)
                }
                0xc3 => {
                    let target = self.memory.read(self.pc + 1);
                    if debug {
                        println!("JMP 0x{target:04x}");
                    }
                    self.pc = target;
                    InstrInfo::jump(3, 4)
                }
                */
                _ => {
                    println!("<Unsupported>");
                    panic!("Unsupported opcode 0x{:02x}", instruction_byte)
                }
            }
        }
        */
    }

    pub fn step(&mut self) {
        let pc = self.get_pc();
        let info = self.decode(pc);
        if let Some(pc_increment) = info.pc_increment {
            assert_eq!(
                info.jumped, false,
                "Only expect to increment PC here when a jump was not taken"
            );
            let pc_reg = self.get_op(OperandName::RegPC);
            pc_reg.write_u16(self, pc + pc_increment);
        }
    }
}

/* Machinery tests */

#[test]
fn test_read_u8() {
    // Given memory initialised to zeroes
    let mut mem = Memory::new();
    mem.write_u8(20, 0xff);
    assert_eq!(mem.read::<u8>(20), 0xff);
    // And the surrounding values are still zero
    assert_eq!(mem.read::<u8>(19), 0x00);
    assert_eq!(mem.read::<u8>(21), 0x00);
}

#[test]
fn test_read_u16() {
    // Given memory initialised to zeroes
    let mut mem = Memory::new();
    // And a u16 stored across two bytes
    mem.write_u8(20, 0x0d);
    mem.write_u8(21, 0x0c);
    // When I read the u16
    // Then its little endian representation is correctly parsed
    assert_eq!(mem.read::<u16>(20), 0x0c0d);
    // And offset memory accesses look correct
    assert_eq!(mem.read::<u16>(19), 0x0d00);
    assert_eq!(mem.read::<u16>(21), 0x000c);
}

#[test]
fn test_read_mem_hl() {
    let mut cpu = CpuState::new();
    cpu.enable_debug();

    cpu.memory.write_u8(0xffcc, 0xab);
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0xcc);
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u8(&cpu), 0xab);
}

#[test]
fn test_write_mem_hl() {
    let mut cpu = CpuState::new();
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0xcc);

    let marker = 0x12;
    cpu.get_op(OperandName::RegsHL).write_u8(&cpu, marker);
    // Then the write shows up directly in memory
    assert_eq!(cpu.memory.read::<u8>(0xffcc), marker);
    // And it shows up in the API for reading (HL)
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u8(&cpu), marker);
}

#[test]
fn test_wide_reg_read() {
    let mut cpu = CpuState::new();
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegC).write_u8(&cpu, 0xcc);
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u16(&cpu), 0xffcc);
}

#[test]
fn test_wide_reg_write() {
    let mut cpu = CpuState::new();
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegC).write_u8(&cpu, 0xcc);

    cpu.get_op(OperandName::RegsBC).write_u16(&cpu, 0xaabb);
    // Then the write shows up in both the individual registers and the wide register
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u16(&cpu), 0xaabb);
    assert_eq!(cpu.get_op(OperandName::RegB).read_u8(&cpu), 0xaa);
    assert_eq!(cpu.get_op(OperandName::RegC).read_u8(&cpu), 0xbb);
}

#[test]
fn test_wide_reg_addressing_mode_read() {
    let mut cpu = CpuState::new();
    // Given BC contains a pointer
    cpu.get_op(OperandName::RegsBC).write_u16(&cpu, 0xaabb);
    // And this pointer contains some data
    cpu.memory.write_u16(0xaabb, 0x23);
    // When we request an unadorned read
    // Then the memory is implicitly dereferenced
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u8(&cpu), 0x23);
}

#[test]
fn test_wide_reg_addressing_mode_deref() {
    let mut cpu = CpuState::new();
    // Given BC contains a pointer
    cpu.get_op(OperandName::RegsBC).write_u16(&cpu, 0xaabb);
    // And this pointer contains some data
    cpu.memory.write_u16(0xaabb, 0x23);

    // When we request a dereferenced read
    // Then we get the dereferenced data
    assert_eq!(
        cpu.get_op(OperandName::RegsBC)
            .read_u8_with_mode(&cpu, AddressingMode::Deref),
        0x23
    );
    // And the contents of the registers are untouched
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u16(&cpu), 0xaabb);
}

#[test]
fn test_wide_reg_addressing_mode_deref_increment() {
    let mut cpu = CpuState::new();
    // Given HL contains a pointer
    cpu.get_op(OperandName::RegsHL).write_u16(&cpu, 0xaabb);
    // And this pointer contains some data
    cpu.memory.write_u16(0xaabb, 0x23);

    // When we request a dereferenced read and pointer increment
    // Then we get the dereferenced data
    assert_eq!(
        cpu.get_op(OperandName::RegsHL)
            .read_u8_with_mode(&cpu, AddressingMode::DerefThenIncrement),
        0x23
    );
    // And the contents of the register are incremented
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u16(&cpu), 0xaabc);
}

#[test]
fn test_wide_reg_addressing_mode_deref_decrement() {
    let mut cpu = CpuState::new();
    // Given HL contains a pointer
    cpu.get_op(OperandName::RegsHL).write_u16(&cpu, 0xaabb);
    // And this pointer contains some data
    cpu.memory.write_u16(0xaabb, 0x23);

    // When we request a dereferenced read and pointer decrement
    // Then we get the dereferenced data
    assert_eq!(
        cpu.get_op(OperandName::RegsHL)
            .read_u8_with_mode(&cpu, AddressingMode::DerefThenDecrement),
        0x23
    );
    // And the contents of the register are decremented
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u16(&cpu), 0xaaba);
}

#[test]
fn test_wide_reg_write_u8() {
    let mut cpu = CpuState::new();
    // Given BC contains a pointer
    cpu.get_op(OperandName::RegsBC).write_u16(&cpu, 0xaabb);
    // And this pointer contains some data
    cpu.memory.write_u16(0xaabb, 0x23);
    // When we request an unadorned write
    cpu.get_op(OperandName::RegsBC).write_u8(&cpu, 0x14);
    // Then the data the pointer points to has been overwritten
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u8(&cpu), 0x14);
}

/* Instructions tests */

/* DEC instruction tests */

#[test]
fn test_dec_reg() {
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let opcode_to_registers = [
        (0x05, OperandName::RegB),
        (0x0d, OperandName::RegC),
        (0x15, OperandName::RegD),
        (0x1d, OperandName::RegE),
        (0x25, OperandName::RegH),
        (0x2d, OperandName::RegL),
        (0x3d, OperandName::RegA),
    ];
    for (opcode, register) in opcode_to_registers {
        cpu.memory.write_u8(0, opcode);

        // Given the register contains 5
        cpu.set_pc(0);
        cpu.get_op(register).write_u8(&cpu, 5);
        cpu.step();
        assert_eq!(cpu.get_op(register).read_u8(&cpu), 4);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given the register contains 0 (underflow)
        cpu.set_pc(0);
        cpu.get_op(register).write_u8(&cpu, 0);
        cpu.step();
        assert_eq!(cpu.get_op(register).read_u8(&cpu), 0xff);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);

        // Given the register contains 1 (zero)
        cpu.set_pc(0);
        cpu.get_op(register).write_u8(&cpu, 1);
        cpu.step();
        assert_eq!(cpu.get_op(register).read_u8(&cpu), 0);
        assert_eq!(cpu.is_flag_set(Flag::Zero), true);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given the register contains 0xf0 (half carry)
        cpu.set_pc(0);
        cpu.get_op(register).write_u8(&cpu, 0xf0);
        cpu.step();
        assert_eq!(cpu.get_op(register).read_u8(&cpu), 0xef);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
    }
}

#[test]
fn test_dec_mem_hl() {
    let mut cpu = CpuState::new();
    // Given the memory pointed to by HL contains 0xf0
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0xcc);
    cpu.get_op(OperandName::RegsHL).write_u8(&cpu, 0xf0);
    // When the CPU runs a DEC (HL) instruction
    // TODO(PT): Check cycle count here
    cpu.memory.write_u8(0, 0x35);
    cpu.step();
    // Then the memory has been decremented
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u8(&cpu), 0xef);
    // And the flags are set correctly
    assert_eq!(cpu.is_flag_set(Flag::Zero), false);
    assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
    assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
}

/* Jump instructions */

#[test]
fn test_jmp() {
    let mut cpu = CpuState::new();
    cpu.memory.write_u8(0, 0xc3);
    // Little endian branch target
    cpu.memory.write_u8(1, 0xfe);
    cpu.memory.write_u8(2, 0xca);
    cpu.step();
    assert_eq!(cpu.get_pc(), 0xcafe);
}

/* LD DstType1, U8 */

#[test]
fn test_ld_reg_u8() {
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let opcode_to_registers = [
        (0x06, OperandName::RegB),
        (0x0e, OperandName::RegC),
        (0x16, OperandName::RegD),
        (0x1e, OperandName::RegE),
        (0x26, OperandName::RegH),
        (0x2e, OperandName::RegL),
        (0x3e, OperandName::RegA),
    ];
    for (opcode, register) in opcode_to_registers {
        cpu.set_pc(0);
        let marker = 0xab;
        cpu.memory.write_u8(0, opcode);
        cpu.memory.write_u8(1, marker);

        // Given the register contains data other than the marker
        cpu.get_op(register).write_u8(&cpu, 0xff);
        cpu.step();
        assert_eq!(cpu.get_op(register).read_u8(&cpu), marker);
    }
}

#[test]
fn test_ld_mem_hl_u8() {
    let mut cpu = CpuState::new();
    cpu.enable_debug();

    // Given the memory pointed to by HL contains 0xf0
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0xcc);
    cpu.get_op(OperandName::RegsHL).write_u8(&cpu, 0xf0);

    // When the CPU runs a LD (HL), u8 instruction
    // TODO(PT): Check cycle count here
    cpu.memory.write_u8(0, 0x36);
    cpu.memory.write_u8(1, 0xaa);
    cpu.step();
    // Then the memory has been assigned
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u8(&cpu), 0xaa);
}

/* Load instruction tests */

#[test]
fn test_ld_b_c() {
    // Given a LD B, C instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let marker = 0xca;
    cpu.get_op(OperandName::RegC).write_u8(&cpu, marker);
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0x00);
    cpu.memory.write_u8(0, 0x41);
    cpu.step();
    // Then the register has been loaded
    assert_eq!(cpu.get_op(OperandName::RegB).read_u8(&cpu), marker);
}

#[test]
fn test_ld_l_l() {
    // Given a LD L, L no-op instruction
    let mut cpu = CpuState::new();
    let marker = 0xfd;
    cpu.get_op(OperandName::RegL).write_u8(&cpu, marker);
    cpu.memory.write_u8(0, 0x6d);
    cpu.step();
    assert_eq!(cpu.get_op(OperandName::RegL).read_u8(&cpu), marker);
}

#[test]
fn test_ld_c_hl() {
    // Given an LD C, (HL) instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0xff);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0xcc);
    let marker = 0xdd;
    cpu.get_op(OperandName::RegsHL).write_u8(&cpu, marker);
    cpu.memory.write_u8(0, 0x4e);
    cpu.step();
    assert_eq!(cpu.get_op(OperandName::RegC).read_u8(&cpu), marker);
}

#[test]
fn test_ld_hl_a() {
    // Given an LD (HL), A instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let marker = 0xaf;
    cpu.get_op(OperandName::RegA).write_u8(&cpu, marker);
    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0x11);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0x22);
    cpu.memory.write_u8(0, 0x77);
    cpu.step();
    assert_eq!(cpu.memory.read::<u8>(0x1122), marker);
}

#[test]
fn test_ld_h_hl() {
    // Given a LD H, (HL) no-op instruction
    let mut cpu = CpuState::new();
    // Set up some sentinel data at the address HL will point to after the instruction
    // TODO(PT): Replace markers with a random u8?
    cpu.memory.write_u8(0xbb22, 0x33);

    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0x11);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0x22);
    cpu.memory.write_u8(0x1122, 0xbb);
    cpu.memory.write_u8(0, 0x66);
    cpu.step();
    // Then the memory load has been applied
    assert_eq!(cpu.get_op(OperandName::RegH).read_u8(&cpu), 0xbb);
    // And dereferencing (HL) now accesses 0xbb22
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u8(&cpu), 0x33);
}

/* XOR [Reg] */

#[test]
fn test_xor_b() {
    // Given a XOR B instruction
    let mut cpu = CpuState::new();
    cpu.get_op(OperandName::RegA).write_u8(&cpu, 0b1110);
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0b0111);

    cpu.memory.write_u8(0, 0xa8);
    cpu.step();

    // Then the XOR has been applied and stored in A
    assert_eq!(cpu.get_op(OperandName::RegA).read_u8(&cpu), 0b1001);
}

#[test]
fn test_xor_mem_hl() {
    // Given a XOR (HL) instruction
    let mut cpu = CpuState::new();
    cpu.get_op(OperandName::RegA).write_u8(&cpu, 0b1110);

    cpu.get_op(OperandName::RegH).write_u8(&cpu, 0x11);
    cpu.get_op(OperandName::RegL).write_u8(&cpu, 0x22);
    cpu.get_op(OperandName::RegsHL).write_u8(&cpu, 0b0111);

    cpu.memory.write_u8(0, 0xae);
    cpu.step();

    // Then the XOR has been applied and stored in A
    assert_eq!(cpu.get_op(OperandName::RegA).read_u8(&cpu), 0b1001);
}

/* LD Dst16, u16 */

#[test]
fn test_ld_dst16_u16_bc() {
    // Given an LD BC, u16 instruction
    let mut cpu = CpuState::new();

    // And B and C contain some data
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0x33);
    cpu.get_op(OperandName::RegC).write_u8(&cpu, 0x44);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x01);
    cpu.memory.write_u16(1, 0xcafe);
    cpu.step();

    // Then the write has been applied to the registers
    assert_eq!(cpu.get_op(OperandName::RegB).read_u8(&cpu), 0xca);
    assert_eq!(cpu.get_op(OperandName::RegC).read_u8(&cpu), 0xfe);
    // And the write shows up in the wide register
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u16(&cpu), 0xcafe);
}

#[test]
fn test_ld_dst16_u16_sp() {
    // Given an LD SP, u16 instruction
    let mut cpu = CpuState::new();

    // And B and C contain some data
    // And SP contains some data
    cpu.get_op(OperandName::RegSP).write_u16(&cpu, 0xffaa);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x31);
    cpu.memory.write_u16(1, 0xcafe);
    cpu.step();

    // Then the write has been applied to the stack pointer
    assert_eq!(cpu.get_op(OperandName::RegSP).read_u16(&cpu), 0xcafe);
}

/* LD A, [Op16] */

#[test]
fn test_ld_a_op16() {
    // Given an LD A, (BC) instruction
    let mut cpu = CpuState::new();

    // And B and C contain some data
    cpu.get_op(OperandName::RegB).write_u8(&cpu, 0x33);
    cpu.get_op(OperandName::RegC).write_u8(&cpu, 0x44);

    // And the address pointed to by BC contains some data
    cpu.memory.write_u8(0x3344, 0xfa);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x0a);
    cpu.step();

    // Then the contents of BC have not been modified
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u16(&cpu), 0x3344);
    // And the data has been copied to A
    assert_eq!(cpu.get_op(OperandName::RegA).read_u8(&cpu), 0xfa);
    // And the memory has not been touched
    assert_eq!(cpu.memory.read::<u8>(0x3344), 0xfa);
}

#[test]
fn test_ld_a_hl_plus() {
    // Given an LD A, (HL+) instruction
    let mut cpu = CpuState::new();

    // And (HL) contains some data
    cpu.get_op(OperandName::RegsHL).write_u16(&cpu, 0x01ff);

    // And the address pointed to by HL contains some data
    cpu.memory.write_u8(0x01ff, 0x56);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x2a);
    cpu.step();

    // Then the pointee has been copied to A
    assert_eq!(cpu.get_op(OperandName::RegA).read_u8(&cpu), 0x56);
    // And HL has been incremented
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u16(&cpu), 0x0200);
    // And the memory has not been touched
    assert_eq!(cpu.memory.read::<u8>(0x01ff), 0x56);
}

/* LD (Reg16), A */

#[test]
fn test_ld_bc_deref_a() {
    // Given an LD (BC), A instruction
    let mut cpu = CpuState::new();

    // And (BC) contains a pointer
    cpu.get_op(OperandName::RegsBC).write_u16(&cpu, 0xabcd);
    // And A contains some data
    cpu.get_op(OperandName::RegA).write_u8(&cpu, 0x11);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x02);
    cpu.step();

    // Then the data in A has been copied to the pointee
    assert_eq!(cpu.memory.read::<u8>(0xabcd), 0x11);
    // And the data shows up when dereferencing the register
    assert_eq!(cpu.get_op(OperandName::RegsBC).read_u8(&cpu), 0x11);
    // And the contents of A have been left untouched
    assert_eq!(cpu.get_op(OperandName::RegA).read_u8(&cpu), 0x11);
}

#[test]
fn test_ld_hl_minus_a() {
    // Given an LD (HL-), A instruction
    let mut cpu = CpuState::new();
    // And HL contains a pointer
    cpu.get_op(OperandName::RegsHL).write_u16(&cpu, 0xabcd);
    // And A contains some data
    cpu.get_op(OperandName::RegA).write_u8(&cpu, 0x11);

    // When the CPU runs the instruction
    cpu.memory.write_u8(0, 0x32);
    cpu.step();

    // Then the data in A has been copied to the pointer
    assert_eq!(cpu.memory.read::<u8>(0xabcd), 0x11);
    // And HL has been decremented
    assert_eq!(cpu.get_op(OperandName::RegsHL).read_u16(&cpu), 0xabcc);
}
