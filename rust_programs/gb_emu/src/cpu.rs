use core::mem;
use std::{
    cell::RefCell,
    collections::BTreeMap,
    env::VarError,
    fmt::{Debug, Display},
};

use alloc::vec::Vec;

use bitmatch::bitmatch;

struct RegisterState {
    a: u8,
    f: u8,
    b: u8,
    c: u8,
    d: u8,
    e: u8,
    h: u8,
    l: u8,
}

impl RegisterState {
    fn new() -> Self {
        Self {
            a: 0,
            f: 0,
            b: 0,
            c: 0,
            d: 0,
            e: 0,
            h: 0,
            l: 0,
        }
    }
}

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

    /*
    fn write<T: ReadSize>(&mut self, offset: u16, value: T) {
        for i in 0..T::BYTE_COUNT {
            //let byte = (value >> (8 * i)) as u8;
            let byte = T::shr(value, 8 * i);
            self.rom[(offset as usize) + i] = T::as_u8(byte);
        }
    }
    */
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

enum Instr {
    NoOp,
    Jmp(u16),
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
    sp: u16,
    pc: u16,
    flags: RefCell<u8>,
    registers: RegisterState,
    registers2: BTreeMap<OperandName, Box<CpuRegister>>,
    mem_hl: Box<DerefHL>,
    memory: Memory,
    debug_enabled: bool,
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum OperandName {
    RegB,
    RegC,
    RegD,
    RegE,
    RegH,
    RegL,
    MemHL,
    RegA,
}

#[derive(Debug, Copy, Clone)]
enum Register {
    B,
    C,
    D,
    E,
    H,
    L,
    A,
}

impl Register {
    fn name(&self) -> &'static str {
        match self {
            Register::B => "B",
            Register::C => "C",
            Register::D => "D",
            Register::E => "E",
            Register::H => "H",
            Register::L => "L",
            Register::A => "A",
        }
    }

    fn from_op_encoded_index(index: u8) -> Self {
        match index {
            0 => Register::B,
            1 => Register::C,
            2 => Register::D,
            3 => Register::E,
            4 => Register::H,
            5 => Register::L,
            7 => Register::A,
            _ => panic!("Not a register"),
        }
    }

    fn get_cpu_ref<'a>(&self, cpu: &'a mut CpuState) -> &'a mut u8 {
        match self {
            Register::B => &mut cpu.registers.b,
            Register::C => &mut cpu.registers.c,
            Register::D => &mut cpu.registers.d,
            Register::E => &mut cpu.registers.e,
            Register::H => &mut cpu.registers.h,
            Register::L => &mut cpu.registers.l,
            Register::A => &mut cpu.registers.a,
        }
    }
}

/*
trait InstrOperand {
    fn read<T: ReadSize>(&self) -> T;
    //fn write_u8(&self
}

impl InstrOperand for CpuRegister {
    fn read<T: ReadSize>(&self) -> T {
        0u8 as T
    }
}

//impl VariableStorage for Cp
*/
trait VariableStorage: Debug + Display {
    fn name(&self) -> &str;
    fn read_u8(&self, cpu: &CpuState) -> u8;
    fn write_u8(&self, cpu: &CpuState, val: u8);
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
    fn name(&self) -> &str {
        &self.name
    }

    fn read_u8(&self, _cpu: &CpuState) -> u8 {
        *self.contents.borrow()
    }

    fn write_u8(&self, _cpu: &CpuState, val: u8) {
        *self.contents.borrow_mut() = val
    }
}

impl Display for CpuRegister {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "[Reg {} (0x{:02x})]", self.name, *self.contents.borrow())
    }
}

#[derive(Debug)]
struct DerefHL {}

impl DerefHL {
    fn new() -> Self {
        Self {}
    }

    fn mem_address(&self, cpu: &CpuState) -> u16 {
        let h = cpu.operand_with_name(OperandName::RegH).read_u8(cpu);
        let l = cpu.operand_with_name(OperandName::RegL).read_u8(cpu);
        let address = ((h as u16) << 8) | (l as u16);
        address
    }
}

impl VariableStorage for DerefHL {
    fn name(&self) -> &str {
        "(HL)"
    }

    fn read_u8(&self, cpu: &CpuState) -> u8 {
        let address = self.mem_address(cpu);
        cpu.memory.read(address)
    }

    fn write_u8(&self, cpu: &CpuState, val: u8) {
        //*self.contents.borrow_mut() = val
        //todo!()
        let address = self.mem_address(cpu);
        cpu.memory.write_u8(address, val);
    }
}

impl Display for DerefHL {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        //write!(f, "[Reg {} (0x{:02x})]", self.name, *self.contents.borrow())
        write!(f, "[Mem {}]", self.name())
    }
}

impl CpuState {
    pub fn new() -> Self {
        let mut registers = BTreeMap::new();
        registers.insert(OperandName::RegB, Box::new(CpuRegister::new("B")));
        registers.insert(OperandName::RegC, Box::new(CpuRegister::new("C")));
        registers.insert(OperandName::RegD, Box::new(CpuRegister::new("D")));
        registers.insert(OperandName::RegE, Box::new(CpuRegister::new("E")));
        registers.insert(OperandName::RegH, Box::new(CpuRegister::new("H")));
        registers.insert(OperandName::RegL, Box::new(CpuRegister::new("L")));
        registers.insert(OperandName::RegA, Box::new(CpuRegister::new("A")));

        Self {
            sp: 0,
            pc: 0,
            flags: RefCell::new(0),
            registers: RegisterState::new(),
            registers2: registers,
            mem_hl: Box::new(DerefHL::new()),
            memory: Memory::new(),
            debug_enabled: false,
        }
    }

    pub fn load_rom_data(&mut self, rom_data: Vec<u8>) {
        // Set initial state
        self.pc = 0x100;
        self.registers.a = 0x01;
        self.set_flags(true, false, true, true);
        self.registers.c = 0x13;
        self.registers.e = 0xd8;
        self.registers.h = 0x01;
        self.registers.e = 0x4d;
        self.sp = 0xfffe;

        let mut rom = self.memory.rom.borrow_mut();
        rom[..rom_data.len()].copy_from_slice(&rom_data);
    }

    pub fn get_pc(&self) -> u16 {
        self.pc
    }

    pub fn enable_debug(&mut self) {
        self.debug_enabled = true;
    }

    pub fn print_regs(&self) {
        println!();
        println!("--- CPU State ---");
        println!("\tPC = 0x{:04x}\tSP = 0x{:04x}", self.pc, self.sp);
        println!(
            "\tA = 0x{:02x}\tF = 0x{:02x}",
            self.registers.a, self.registers.f
        );
        println!(
            "\tB = 0x{:02x}\tC = 0x{:02x}",
            self.registers.b, self.registers.c
        );
        println!(
            "\tD = 0x{:02x}\tE = 0x{:02x}",
            self.registers.d, self.registers.e
        );
        println!(
            "\tH = 0x{:02x}\tL = 0x{:02x}",
            self.registers.h, self.registers.l
        );
    }

    fn get_register_ref(&mut self, reg: Register) -> &mut u8 {
        match reg {
            Register::B => &mut self.registers.b,
            Register::C => &mut self.registers.c,
            Register::D => &mut self.registers.d,
            Register::E => &mut self.registers.e,
            Register::H => &mut self.registers.h,
            Register::L => &mut self.registers.l,
            Register::A => &mut self.registers.a,
        }
    }

    pub fn get_register(&mut self, register: Register) -> u8 {
        *self.get_register_ref(register)
    }

    pub fn set_register(&mut self, register: Register, val: u8) {
        *self.get_register_ref(register) = val;
    }

    fn get_a(&self) -> u8 {
        self.registers.a
    }

    fn get_bc(&self) -> u16 {
        self.registers.b as u16 | ((self.registers.c as u16) << 8)
    }

    fn get_hl(&self) -> u16 {
        self.registers.h as u16 | ((self.registers.l as u16) << 8)
    }

    fn set_hl(&mut self, val: u16) {
        self.registers.h = val as u8;
        self.registers.l = (val >> 8) as u8;
    }

    fn set_flags(&mut self, z: bool, n: bool, h: bool, c: bool) {
        let high_nibble = c as u8 | ((h as u8) << 1) | ((n as u8) << 2) | ((z as u8) << 3);
        self.registers.f = high_nibble << 4;
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
        //(self.registers.f & (1 << (4 + flag_bit_index))) != 0
        let flags = *self.flags.borrow();
        (flags & (1 << (4 + flag_bit_index))) != 0
    }

    pub fn format_flags(&self) -> String {
        format!(
            "{}{}{}{}",
            if self.is_flag_set(Flag::Zero) {
                "Z"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::Subtract) {
                "N"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::HalfCarry) {
                "H"
            } else {
                "-"
            },
            if self.is_flag_set(Flag::Carry) {
                "C"
            } else {
                "-"
            }
        )
    }

    pub fn operand_name_from_lookup_index(&self, index: u8) -> OperandName {
        match index {
            6 => OperandName::MemHL,
            0 => OperandName::RegB,
            1 => OperandName::RegC,
            2 => OperandName::RegD,
            3 => OperandName::RegE,
            4 => OperandName::RegH,
            5 => OperandName::RegL,
            7 => OperandName::RegA,
            _ => panic!("Invalid index"),
        }
    }

    pub fn storage_from_lookup_index(&self, index: u8) -> &dyn VariableStorage {
        let operand_name = self.operand_name_from_lookup_index(index);
        match operand_name {
            OperandName::MemHL => &*self.mem_hl as _,
            _ => &*self.registers2[&operand_name] as _,
        }
    }

    pub fn operand_with_name(&self, name: OperandName) -> &dyn VariableStorage {
        match name {
            OperandName::MemHL => &*self.mem_hl as _,
            _ => &*self.registers2[&name],
        }
    }

    #[bitmatch]
    pub fn decode(&mut self, pc: u16) -> InstrInfo {
        // Fetch the next opcode
        let instruction_byte: u8 = self.memory.read(pc);
        // Decode using the strategy described by https://gb-archive.github.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html
        let opcode_digit1 = (instruction_byte >> 6) & 0b11;
        let opcode_digit2 = (instruction_byte >> 3) & 0b111;
        let opcode_digit3 = (instruction_byte >> 0) & 0b111;

        let debug = self.debug_enabled;
        if debug {
            print!("0x{:04x}\t{:02x}\t", self.pc, instruction_byte);
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
            },
            0x76 => {
                panic!("HALT not implemented")
            },
            0xc3 => {
                let target = self.memory.read(self.pc + 1);
                if debug {
                    println!("JMP 0x{target:04x}");
                }
                self.pc = target;
                Some(InstrInfo::jump(3, 4))
            },
            // Handled down below
            _ => None
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
                let op = self.storage_from_lookup_index(i);
                if debug {
                    print!("DEC {op}\t");
                }
                let prev = op.read_u8(&self);
                let new = prev - 1;
                op.write_u8(&self, new);
                self.update_flag(FlagUpdate::Zero(new == 0));
                self.update_flag(FlagUpdate::Subtract(true));
                // Underflow into the high nibble?
                self.update_flag(FlagUpdate::HalfCarry((prev & 0xf) < (new & 0xf)));

                if debug {
                    println!(
                        "Result: {op}"
                    )
                }
                InstrInfo::seq(1, 1)
                // TODO(PT): The commented expression is for half-carry addition
                //half_carry_flag = (((prev & 0xf) + (new_value & 0xf)) & 0x10) == 0x10;
            },
            "01tttfff" => {
                // Opcode is 0x40 to 0x7f
                // LD [Reg], [Reg]
                let from_op = self.storage_from_lookup_index(f);
                let from_val = from_op.read_u8(self);
                let to_op = self.storage_from_lookup_index(t);

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
                    self.operand_name_from_lookup_index(f),
                    self.operand_name_from_lookup_index(f),
                ];
                let cycle_count = if operand_names.contains(&OperandName::MemHL) {
                    2
                } else {
                    1
                };
                InstrInfo::seq(1, cycle_count)
            },
            "00iii110" => {
                // LD [Reg], [u8]
                let dest = self.storage_from_lookup_index(opcode_digit2);
                let val = self.memory.read(self.pc + 1);
                dest.write_u8(&self, val);
                if debug {
                    println!("LD {dest} with 0x{val:02x}");
                }
                // TODO(PT): Update me when the operand is (HL)
                InstrInfo::seq(2, 2)
            }
            _ => panic!("Unsupported")
        }

        /*
        } else if opcode_digit1 == 0b00 && opcode_digit3 == 0b110 {
        } else {
            match instruction_byte {
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
                _ => {
                    println!("<Unsupported>");
                    panic!("Unsupported opcode 0x{:02x}", instruction_byte)
                }
            }
        }
        */
    }

    pub fn step(&mut self) {
        let info = self.decode(self.pc);
        if let Some(pc_increment) = info.pc_increment {
            assert_eq!(
                info.jumped, false,
                "Only expect to increment PC here when a jump was not taken"
            );
            self.pc += pc_increment;
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
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0xff);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0xcc);
    assert_eq!(
        cpu.operand_with_name(OperandName::MemHL).read_u8(&cpu),
        0xab
    );
}

#[test]
fn test_write_mem_hl() {
    let mut cpu = CpuState::new();
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0xff);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0xcc);

    let marker = 0x12;
    cpu.operand_with_name(OperandName::MemHL)
        .write_u8(&cpu, marker);
    // Then the write shows up directly in memory
    assert_eq!(cpu.memory.read::<u8>(0xffcc), marker);
    // And it shows up in the API for reading (HL)
    assert_eq!(
        cpu.operand_with_name(OperandName::MemHL).read_u8(&cpu),
        marker
    );
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
        cpu.pc = 0;
        cpu.operand_with_name(register).write_u8(&cpu, 5);
        cpu.step();
        assert_eq!(cpu.operand_with_name(register).read_u8(&cpu), 4);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given the register contains 0 (underflow)
        cpu.pc = 0;
        cpu.operand_with_name(register).write_u8(&cpu, 0);
        cpu.step();
        assert_eq!(cpu.operand_with_name(register).read_u8(&cpu), 0xff);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);

        // Given the register contains 1 (zero)
        cpu.pc = 0;
        cpu.operand_with_name(register).write_u8(&cpu, 1);
        cpu.step();
        assert_eq!(cpu.operand_with_name(register).read_u8(&cpu), 0);
        assert_eq!(cpu.is_flag_set(Flag::Zero), true);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given the register contains 0xf0 (half carry)
        cpu.pc = 0;
        cpu.operand_with_name(register).write_u8(&cpu, 0xf0);
        cpu.step();
        assert_eq!(cpu.operand_with_name(register).read_u8(&cpu), 0xef);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
    }
}

#[test]
fn test_dec_mem_hl() {
    let mut cpu = CpuState::new();
    // Given the memory pointed to by HL contains 0xf0
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0xff);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0xcc);
    cpu.operand_with_name(OperandName::MemHL)
        .write_u8(&cpu, 0xf0);
    // When the CPU runs a DEC (HL) instruction
    // TODO(PT): Check cycle count here
    cpu.memory.write_u8(0, 0x35);
    cpu.step();
    // Then the memory has been decremented
    assert_eq!(cpu.operand_with_name(OperandName::MemHL).read_u8(&cpu), 0xef);
    // And the flags are set correctly
    assert_eq!(cpu.is_flag_set(Flag::Zero), false);
    assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
    assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
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
        cpu.pc = 0;
        let marker = 0xab;
        cpu.memory.write_u8(0, opcode);
        cpu.memory.write_u8(1, marker);

        // Given the register contains data other than the marker
        cpu.operand_with_name(register).write_u8(&cpu, 0xff);
        cpu.step();
        assert_eq!(cpu.operand_with_name(register).read_u8(&cpu), marker);
    }
}

#[test]
fn test_ld_mem_hl_u8() {
    let mut cpu = CpuState::new();
    cpu.enable_debug();

    // Given the memory pointed to by HL contains 0xf0
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0xff);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0xcc);
    cpu.operand_with_name(OperandName::MemHL)
        .write_u8(&cpu, 0xf0);

    // When the CPU runs a LD (HL), u8 instruction
    // TODO(PT): Check cycle count here
    cpu.memory.write_u8(0, 0x36);
    cpu.memory.write_u8(1, 0xaa);
    cpu.step();
    // Then the memory has been assigned
    assert_eq!(cpu.operand_with_name(OperandName::MemHL).read_u8(&cpu), 0xaa);
}

#[test]
fn test_jmp() {
    let mut cpu = CpuState::new();
    cpu.memory.write_u8(0, 0xc3);
    // Little endian branch target
    cpu.memory.write_u8(1, 0xfe);
    cpu.memory.write_u8(2, 0xca);
    cpu.step();
    assert_eq!(cpu.pc, 0xcafe);
}

/* Load instruction tests */

#[test]
fn test_ld_b_c() {
    // Given a LD B, C instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let marker = 0xca;
    cpu.operand_with_name(OperandName::RegC)
        .write_u8(&cpu, marker);
    cpu.operand_with_name(OperandName::RegB)
        .write_u8(&cpu, 0x00);
    cpu.memory.write_u8(0, 0x41);
    cpu.step();
    // Then the register has been loaded
    assert_eq!(
        cpu.operand_with_name(OperandName::RegB).read_u8(&cpu),
        marker
    );
}

#[test]
fn test_ld_l_l() {
    // Given a LD L, L no-op instruction
    let mut cpu = CpuState::new();
    let marker = 0xfd;
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, marker);
    cpu.memory.write_u8(0, 0x6d);
    cpu.step();
    assert_eq!(
        cpu.operand_with_name(OperandName::RegL).read_u8(&cpu),
        marker
    );
}

#[test]
fn test_ld_c_hl() {
    // Given an LD C, (HL) instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0xff);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0xcc);
    let marker = 0xdd;
    cpu.operand_with_name(OperandName::MemHL).write_u8(&cpu, marker);
    cpu.memory.write_u8(0, 0x4e);
    cpu.step();
    assert_eq!(
        cpu.operand_with_name(OperandName::RegC).read_u8(&cpu),
        marker
    );
}

#[test]
fn test_ld_hl_a() {
    // Given an LD (HL), A instruction
    let mut cpu = CpuState::new();
    cpu.enable_debug();
    let marker = 0xaf;
    cpu.operand_with_name(OperandName::RegA)
        .write_u8(&cpu, marker);
    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0x11);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0x22);
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

    cpu.operand_with_name(OperandName::RegH)
        .write_u8(&cpu, 0x11);
    cpu.operand_with_name(OperandName::RegL)
        .write_u8(&cpu, 0x22);
    cpu.memory.write_u8(0x1122, 0xbb);
    cpu.memory.write_u8(0, 0x66);
    cpu.step();
    // Then the memory load has been applied
    assert_eq!(cpu.operand_with_name(OperandName::RegH).read_u8(&cpu), 0xbb);
    // And dereferencing (HL) now accesses 0xbb22
    //assert_eq!(cpu.operand_with_name(OperandName::RegL).read_u8(&cpu), marker);
}
