use core::mem;

use alloc::vec::Vec;


struct RegisterState {
    registers: Vec<u8>,
}

impl RegisterState {
    fn new() -> Self {
        RegisterState {
            registers: Vec::new(),
        }
    }
}

trait ReadSize {
    const BYTE_COUNT: usize;
    fn from(val: usize) -> Self;
}

impl ReadSize for u8 {
    const BYTE_COUNT: usize = mem::size_of::<u8>();
    fn from(val: usize) -> Self {
        val.try_into().expect("u8 overflow")
    }
}
impl ReadSize for u16 {
    const BYTE_COUNT: usize = mem::size_of::<u16>();
    fn from(val: usize) -> Self {
        val.try_into().expect("u16 overflow")
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
            //let i_as_u16 = i as u16;
            //val |= (offset + i_as_u16) << (i_as_u16 * 8);
            let byte = self.rom[(offset as usize) + i];
            val |= (byte as usize) << (i * 8);
        }
        T::from(val)
    }
}

pub struct CpuState {
    sp: u16,
    pc: u16,
    registers: RegisterState,
    memory: Memory,
}

impl CpuState {
    pub fn new() -> Self {
        Self {
            sp: 0,
            pc: 0x100,
            registers: RegisterState::new(),
            memory: Memory::new(),
        }
    }

    pub fn step(&mut self) {
        // Fetch the next opcode
        let instruction_byte: u8 = self.memory.read(self.pc);
        println!("instruction byte: {instruction_byte:x}");
        match instruction_byte {
            0x00 => (),
            _ => panic!("Unsupported instruction {:x}", instruction_byte)
        }
        self.pc += 1;
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
