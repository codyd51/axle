use core::mem;

use alloc::vec::Vec;


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
            a:  0,
            f:  0,
            b:  0,
            c:  0,
            d:  0,
            e:  0,
            h:  0,
            l:  0,
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
    rom: [u8; 0xffff],
}

impl Memory {
    fn new() -> Self {
        Self {
            rom: [0; 0xffff],
        }
    }

    fn read<T: ReadSize>(&self, offset: u16) -> T {
        let mut val = 0usize;
        for i in 0..T::BYTE_COUNT {
            let byte = self.rom[(offset as usize) + i];
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
    fn write_u8(&mut self, offset: u16, value: u8) {
        self.rom[offset as usize] = value;
    }
    fn write_u16(&mut self, offset: u16, value: u16) {
        self.rom[offset as usize] = value as u8;
        self.rom[(offset + 1) as usize] = (value >> 8) as u8;
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
    jumped: bool
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
            jumped: true
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
    registers: RegisterState,
    memory: Memory,
    debug_enabled: bool,
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
            _ => panic!("Not a register")
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

impl CpuState {
    pub fn new() -> Self {
        Self {
            sp: 0,
            pc: 0,
            registers: RegisterState::new(),
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

        self.memory.rom[..rom_data.len()].copy_from_slice(&rom_data);
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
        println!("\tA = 0x{:02x}\tF = 0x{:02x}", self.registers.a, self.registers.f);
        println!("\tB = 0x{:02x}\tC = 0x{:02x}", self.registers.b, self.registers.c);
        println!("\tD = 0x{:02x}\tE = 0x{:02x}", self.registers.d, self.registers.e);
        println!("\tH = 0x{:02x}\tL = 0x{:02x}", self.registers.h, self.registers.l);
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

    pub fn get_register(&mut self, register: Register) -> u8{
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

    fn update_flag(&mut self, flag: FlagUpdate) {
        let mut flag_setting_and_bit_index = match flag {
            FlagUpdate::Zero(on) => (on, 3),
            FlagUpdate::Subtract(on) => (on, 2),
            FlagUpdate::HalfCarry(on) => (on, 1),
            FlagUpdate::Carry(on) => (on, 0),
        };
        // Flags are always in the high nibble
        let bit_index = 4 + flag_setting_and_bit_index.1;
        if flag_setting_and_bit_index.0 {
            // Enable flag
            self.registers.f |= 1 << bit_index;
        }
        else {
            // Disable flag
            self.registers.f &= !(1 << bit_index);
        }
    }

    fn is_flag_set(&self, flag: Flag) -> bool {
        let mut flag_bit_index = match flag {
            Flag::Zero => 3,
            Flag::Subtract => 2,
            Flag::HalfCarry => 1,
            Flag::Carry => 0,
        };
        (self.registers.f & (1 << (4 + flag_bit_index))) != 0
    }

    pub fn format_flags(&self) -> String {
        format!("{}{}{}{}", 
        if self.is_flag_set(Flag::Zero) {"Z"} else {"-"},
        if self.is_flag_set(Flag::Subtract) {"N"} else {"-"},
        if self.is_flag_set(Flag::HalfCarry) {"H"} else {"-"},
        if self.is_flag_set(Flag::Carry) {"C"} else {"-"})
    }

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
            print!("{:02x} {:02x} {:02x}\t", opcode_digit1, opcode_digit2, opcode_digit3);
        }

        // Some classes of instructions can be handled as a group
        // PT: Manually derived by inspecting the opcode table
        // Opcode table ref: https://meganesulli.com/generate-gb-opcodes/
        if opcode_digit1 == 0b00 && opcode_digit3 == 0b101 {
            // DEC [reg]
            let reg = Register::from_op_encoded_index(opcode_digit2);
            /*
            // TODO(PT): What happens when we get [HL], an unsupported pattern for DEC [reg]?
            if operand == Operand::MemHL {
                todo!();
            }
            */
            // PT: Enter a new scope to modify the register value
            let zero_flag;
            let half_carry_flag;
            let new_value;
            {
                let mut register_ref = reg.get_cpu_ref(self);

                // Update the register value
                let prev = *register_ref;
                new_value = prev - 1;
                *register_ref = new_value;

                zero_flag = new_value == 0;
                // TODO(PT): The commented expression is for half-carry addition
                //half_carry_flag = (((prev & 0xf) + (new_value & 0xf)) & 0x10) == 0x10;

                // Underflow into the high nibble?
                half_carry_flag = (prev & 0xf) < (new_value & 0xf);
            }
            self.update_flag(FlagUpdate::Zero(zero_flag));
            self.update_flag(FlagUpdate::Subtract(true));
            self.update_flag(FlagUpdate::HalfCarry(half_carry_flag));

            if debug {
                let reg_name = reg.name();
                println!(
                    "{reg_name} -= 1;\t{reg_name} = 0x{:02x}\t{}", new_value, self.format_flags()
                );
            }
            
            InstrInfo::seq(1, 1)
        }
        else if opcode_digit1 == 0b00 && opcode_digit3 == 0b110 {
            // LD [Reg], [u8]
            let reg = Register::from_op_encoded_index(opcode_digit2);
            let new_value = self.memory.read(self.pc + 1);
            let reg_ref = reg.get_cpu_ref(self);
            *reg_ref = new_value;
            if debug { 
                println!("{} = 0x{:02x}", reg.name(), reg_ref); 
            }
            InstrInfo::seq(2, 2)
        }
        else {
            match instruction_byte {
                0x00 => {
                    if debug { println!("NOP"); }
                    InstrInfo::seq(1, 1)
                },
                0x02 => {
                    let bc = self.get_bc();
                    let mem = self.memory.read(bc);
                    self.registers.a = mem;
                    if debug { println!("A = [BC] ({bc:04x}: {mem:02x})"); }
                    InstrInfo::seq(1, 2)
                },
                0x0c => {
                    self.registers.c += 1;
                    if debug { println!("C += 1"); }
                    InstrInfo::seq(1, 1)
                },
                0x20 => {
                    if !self.is_flag_set(Flag::Zero) {
                        let rel_target: i8 = self.memory.read(self.pc + 1);
                        if debug { println!("JR NZ +{:02x};\t(taken)", rel_target); }
                        // Add 2 to pc before doing the relative target, as 
                        // this instruction is 2 bytes wide
                        self.pc += 2;
                        self.pc += rel_target as u16;
                        InstrInfo::jump(2, 3)
                    }
                    else {
                        if debug { println!("JR NZ +off;\t(not taken)"); }
                        InstrInfo::seq(2, 2)
                    }
                }
                0x21 => {
                    self.set_hl(self.memory.read(self.pc + 1));
                    if debug { println!("HL = 0x{:04x}", self.get_hl()); }
                    InstrInfo::seq(3, 3)
                },
                0x32 => {
                    let hl = self.get_hl();
                    self.memory.write_u8(hl, self.get_a());
                    self.set_hl(hl - 1);
                    if debug { println!("LD (HL-) = A;\t*0x{:04x} = 0x{:02x}", hl, self.get_a()); }
                    InstrInfo::seq(1, 2)
                },
                0xaf => {
                    self.registers.a ^= self.registers.a;
                    if debug { println!("A ^= A"); }
                    self.set_flags(true, false, false, false);
                    InstrInfo::seq(1, 1)
                },
                0xc3 => {
                    let target = self.memory.read(self.pc + 1);
                    if debug { println!("JMP 0x{target:04x}"); }
                    self.pc = target;
                    InstrInfo::jump(3, 4)
                },
                _ => {
                    println!("<Unsupported>");
                    panic!("Unsupported opcode 0x{:02x}", instruction_byte)
                },
            }
        }
    }

    pub fn step(&mut self) {
        let info = self.decode(self.pc);
        if let Some(pc_increment) = info.pc_increment {
            assert_eq!(info.jumped, false, "Only expect to increment PC here when a jump was not taken");
            self.pc += pc_increment;
        }
    }
}

#[test]
fn test_read_u8() {
    // Given memory initialised to zeroes
    let mut mem = Memory::new();
    mem.rom[20] = 0xff;
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
    mem.rom[20] = 0x0d;
    mem.rom[21] = 0x0c;
    // When I read the u16
    // Then its little endian representation is correctly parsed
    assert_eq!(mem.read::<u16>(20), 0x0c0d);
    // And offset memory accesses look correct
    assert_eq!(mem.read::<u16>(19), 0x0d00);
    assert_eq!(mem.read::<u16>(21), 0x000c);
}

#[test]
fn test_dec_reg() {
    let mut cpu = CpuState::new();
    let opcode_to_registers = [
        (0x05, Register::B),
        (0x0d, Register::C),
        (0x15, Register::D),
        (0x1d, Register::E),
        (0x25, Register::H),
        (0x2d, Register::L),
        (0x3d, Register::A),
    ];
    for (opcode, register) in opcode_to_registers {
        cpu.memory.rom[0] = opcode;

        // Given B contains 5
        cpu.pc = 0;
        cpu.set_register(register, 5);
        cpu.step();
        assert_eq!(cpu.get_register(register), 4);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given B contains 0 (underflow)
        cpu.pc = 0;
        cpu.set_register(register, 0);
        cpu.step();
        assert_eq!(cpu.get_register(register), 0xff);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);

        // Given B contains 1 (zero)
        cpu.pc = 0;
        cpu.set_register(register, 1);
        cpu.step();
        assert_eq!(cpu.get_register(register), 0);
        assert_eq!(cpu.is_flag_set(Flag::Zero), true);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), false);

        // Given B contains 0xf0 (half carry)
        cpu.pc = 0;
        cpu.set_register(register, 0xf0);
        cpu.step();
        assert_eq!(cpu.get_register(register), 0xef);
        assert_eq!(cpu.is_flag_set(Flag::Zero), false);
        assert_eq!(cpu.is_flag_set(Flag::Subtract), true);
        assert_eq!(cpu.is_flag_set(Flag::HalfCarry), true);
    }
}

#[test]
fn test_ld_reg_u8() {
    let mut cpu = CpuState::new();
    let opcode_to_registers = [
        (0x06, Register::B),
        (0x0e, Register::C),
        (0x16, Register::D),
        (0x1e, Register::E),
        (0x26, Register::H),
        (0x2e, Register::L),
        (0x3e, Register::A),
    ];
    for (opcode, register) in opcode_to_registers {
        cpu.pc = 0;
        let marker = 0xab;
        cpu.memory.rom[0] = opcode;
        cpu.memory.rom[1] = marker;

        // Given the register contains data other than the marker
        cpu.set_register(register, 0xff);
        cpu.step();
        assert_eq!(cpu.get_register(register), marker);
    }
}

#[test]
fn test_jmp() {
    let mut cpu = CpuState::new();
    cpu.memory.rom[0] = 0xc3;
    // Little endian branch target
    cpu.memory.rom[1] = 0xfe;
    cpu.memory.rom[2] = 0xca;
    cpu.step();
    assert_eq!(cpu.pc, 0xcafe);
}
