use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use alloc::{format, vec};
use core::cell::{Ref, RefCell};
use core::fmt::{Debug, Display, Formatter};
use core::mem;
use std::io;
use std::io::Write;

use compilation_definitions::encoding::ModRmByte;
use derive_more::Constructor;
use strum::IntoEnumIterator;

use compilation_definitions::instructions::{
    AddRegToReg, CompareImmWithReg, CompareRegWithReg, DivRegByReg, Instr, InstrBytecodeProvider,
    InstrContinuation, InstrDisassembler, InstrInfo, MoveImmToReg, MoveRegToReg, MulRegByReg,
    SubRegFromReg,
};
use compilation_definitions::prelude::*;

#[repr(C)]
#[derive(Debug)]
struct ElfHeader64 {
    magic: [u8; 4],
    word_size: u8,
    endianness: u8,
    elf_version: u8,
    os_abi: u8,
    os_abi_version: u8,
    reserved: [u8; 7],
    elf_type: u16,
    isa_type: u16,
    elf_version2: u32,
    entry_point: u64,
    program_header_table_start: u64,
    section_header_table_start: u64,
    flags: u32,
    header_size: u16,
    program_header_table_entry_size: u16,
    program_header_table_entry_count: u16,
    section_header_table_entry_size: u16,
    section_header_table_entry_count: u16,
    section_names_section_header_index: u16,
}
assert_eq_size!(ElfHeader64, [u8; 64]);

#[repr(C)]
#[derive(Debug)]
struct ElfSegment64 {
    segment_type: u32,
    flags: u32,
    offset: u64,
    vaddr: u64,
    paddr: u64,
    file_size: u64,
    mem_size: u64,
    align: u64,
}
assert_eq_size!(ElfSegment64, [u8; 56]);

/*
#[derive(Debug)]
pub struct InstrInfo {
    pub instruction_size: usize,
    rip_increment: Option<usize>,
    jumped: bool,
    // PT: Just a hack for testing
    pub did_return: bool,
}

impl InstrInfo {
    fn seq(instruction_size: usize) -> Self {
        Self {
            instruction_size,
            rip_increment: Some(instruction_size),
            jumped: false,
            did_return: false,
        }
    }
    fn jump(instruction_size: usize) -> Self {
        Self {
            instruction_size,
            // Don't increment rip because the instruction will modify rip directly
            rip_increment: None,
            jumped: true,
            did_return: false,
        }
    }
}
*/

#[derive(Debug, PartialEq, Copy, Clone)]
enum Flag {
    Carry,
    Parity,
    AuxiliaryCarry,
    Zero,
    Sign,
    Overflow,
}

enum FlagUpdate {
    Carry(bool),
    Parity(bool),
    AuxiliaryCarry(bool),
    Zero(bool),
    Sign(bool),
    Overflow(bool),
}

#[derive(Copy, Clone, Debug)]
enum FlagCondition {
    NoCarry,
    Carry,
    ParityOdd,
    ParityEven,
    NoAuxCarry,
    AuxCarry,
    NotZero,
    Zero,
    SignPositive,
    SignNegative,
    NotOverflow,
    Overflow,
}

pub trait VariableStorage: Debug + Display {
    fn display_name(&self) -> &str;

    fn read_u8(&self, machine: &MachineState) -> u8;
    fn read_u16(&self, _machine: &MachineState) -> u16;
    fn read_u32(&self, _machine: &MachineState) -> u32;
    fn read_u64(&self, _machine: &MachineState) -> u64;

    fn write_u8(&self, machine: &MachineState, val: u8);
    fn write_u16(&self, machine: &MachineState, val: u16);
    fn write_u32(&self, machine: &MachineState, val: u32);
    fn write_u64(&self, machine: &MachineState, val: u64);
}

#[derive(Debug)]
struct CpuRegister {
    reg: Register,
    display_name: String,
    contents: RefCell<u64>,
}

impl CpuRegister {
    fn new(reg: Register) -> Self {
        Self {
            reg,
            display_name: format!("{reg:?}"),
            contents: RefCell::new(0),
        }
    }
}

impl Display for CpuRegister {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        todo!()
    }
}

impl VariableStorage for CpuRegister {
    fn display_name(&self) -> &str {
        todo!()
    }

    fn read_u8(&self, machine: &MachineState) -> u8 {
        *self.contents.borrow() as u8
    }

    fn read_u16(&self, _machine: &MachineState) -> u16 {
        *self.contents.borrow() as u16
    }

    fn read_u32(&self, _machine: &MachineState) -> u32 {
        *self.contents.borrow() as u32
    }

    fn read_u64(&self, _machine: &MachineState) -> u64 {
        *self.contents.borrow() as u64
    }

    fn write_u8(&self, machine: &MachineState, val: u8) {
        let mut current_val = *self.contents.borrow();
        let mut as_bytes = current_val.to_le_bytes();
        as_bytes[0] = val as u8;
        *self.contents.borrow_mut() = u64::from_le_bytes(as_bytes);
    }

    fn write_u16(&self, machine: &MachineState, val: u16) {
        let mut current_val = *self.contents.borrow();
        let mut current_as_bytes = current_val.to_ne_bytes();
        let val_as_bytes = val.to_ne_bytes();
        current_as_bytes[0..val_as_bytes.len()].copy_from_slice(&val_as_bytes);
        *self.contents.borrow_mut() = u64::from_le_bytes(current_as_bytes);
    }

    fn write_u32(&self, machine: &MachineState, val: u32) {
        let mut current_val = *self.contents.borrow();
        let mut current_as_bytes = current_val.to_ne_bytes();
        let val_as_bytes = val.to_ne_bytes();
        current_as_bytes[0..val_as_bytes.len()].copy_from_slice(&val_as_bytes);
        *self.contents.borrow_mut() = u64::from_le_bytes(current_as_bytes);
    }

    fn write_u64(&self, machine: &MachineState, val: u64) {
        *self.contents.borrow_mut() = val
    }
}

#[derive(Debug)]
pub struct CpuRegisterView<'a> {
    view: RegView,
    reg: &'a CpuRegister,
}

impl<'a> CpuRegisterView<'a> {
    fn new(view: &RegView, reg: &'a CpuRegister) -> Self {
        Self { view: *view, reg }
    }

    fn read(&self, machine: &MachineState) -> usize {
        match self.view.1 {
            AccessType::L => self.reg.read_u8(machine) as _,
            AccessType::H => (self.reg.read_u16(machine) >> 8) as _,
            AccessType::X => self.reg.read_u16(machine) as _,
            AccessType::EX => self.reg.read_u32(machine) as _,
            AccessType::RX => self.reg.read_u64(machine) as _,
        }
    }

    fn write(&self, machine: &MachineState, val: usize) {
        match self.view.1 {
            AccessType::L => self.reg.write_u8(machine, val as _),
            AccessType::H => {
                // Read the current low u16, write the high byte, and write it back
                let low_u16 = self.reg.read_u16(machine);
                let mut as_bytes = low_u16.to_le_bytes();
                as_bytes[1] = val as u8;
                self.reg.write_u16(machine, u16::from_le_bytes(as_bytes))
            }
            AccessType::X => self.reg.write_u16(machine, val as _),
            AccessType::EX => self.reg.write_u32(machine, val as _),
            AccessType::RX => self.reg.write_u64(machine, val as _),
        }
    }
}

// Ref: https://stackoverflow.com/questions/25428920/how-to-get-a-slice-as-an-array-in-rust
fn clone_into_array<A, T>(slice: &[T]) -> A
where
    A: Default + AsMut<[T]>,
    T: Clone,
{
    let mut a = A::default();
    <A as AsMut<[T]>>::as_mut(&mut a).clone_from_slice(slice);
    a
}

#[derive(Debug)]
struct VirtualMemoryRegion {
    base: u64,
    size: u64,
    store: RefCell<Vec<u8>>,
}

impl VirtualMemoryRegion {
    fn new(base: u64, size: u64) -> Self {
        Self {
            base,
            size,
            store: RefCell::new(vec![0; size as usize]),
        }
    }

    fn new_with_contents(base: u64, contents: &[u8]) -> Self {
        Self {
            base,
            size: contents.len() as _,
            store: RefCell::new(contents.to_vec()),
        }
    }

    fn contains(&self, addr: u64) -> bool {
        addr >= self.base && addr < self.base + self.size
    }

    fn read_u8(&self, virtual_addr: u64) -> u8 {
        let translated_addr = virtual_addr - self.base;
        let store = self.store.borrow();
        store[translated_addr as usize]
    }

    fn read_u16(&self, addr: u64) -> u16 {
        todo!()
    }

    fn read_u32(&self, virtual_addr: u64) -> u32 {
        let translated_addr = virtual_addr - self.base;
        let store = self.store.borrow();
        let bytes = clone_into_array(
            &store[(translated_addr as _)..(translated_addr as usize + mem::size_of::<u32>())],
        );
        u32::from_ne_bytes(bytes)
    }

    fn read_u64(&self, addr: u64) -> u64 {
        todo!()
    }

    fn write_u8(&self, addr: u64, val: u8) {
        todo!()
    }

    fn write_u16(&self, addr: u64, val: u16) {
        todo!()
    }

    fn write_u32(&self, virtual_addr: u64, val: u32) {
        let translated_addr = virtual_addr - self.base;
        let mut store = self.store.borrow_mut();
        for (i, b) in val.to_ne_bytes().iter().enumerate() {
            store[(translated_addr as usize) + i] = *b;
        }
    }

    fn write_u64(&self, addr: u64, val: u64) {
        todo!()
    }
}

#[derive(Debug)]
struct Ram {
    regions: RefCell<Vec<VirtualMemoryRegion>>,
}

impl Ram {
    fn new() -> Self {
        Self {
            regions: RefCell::new(vec![]),
        }
    }

    fn add_region(&self, virtual_memory_region: VirtualMemoryRegion) {
        self.regions.borrow_mut().push(virtual_memory_region)
    }

    fn region_containing_addr<'a>(
        &self,
        addr: u64,
        regions: &'a Ref<Vec<VirtualMemoryRegion>>,
    ) -> &'a VirtualMemoryRegion {
        for region in regions.iter() {
            if region.contains(addr) {
                return region;
            }
        }
        panic!("No region contains 0x{addr:x}");
    }

    pub fn read_u8(&self, addr: u64) -> u8 {
        let regions = self.regions.borrow();
        let region = self.region_containing_addr(addr, &regions);
        region.read_u8(addr)
    }

    fn read_u16(&self, addr: u64) -> u16 {
        todo!()
    }

    fn read_u32(&self, addr: u64) -> u32 {
        let regions = self.regions.borrow();
        let region = self.region_containing_addr(addr, &regions);
        region.read_u32(addr)
    }

    fn read_u64(&self, addr: u64) -> u64 {
        todo!()
    }

    fn write_u8(&self, addr: u64, val: u8) {
        todo!()
    }

    fn write_u16(&self, addr: u64, val: u16) {
        todo!()
    }

    fn write_u32(&self, addr: u64, val: u32) {
        let regions = self.regions.borrow();
        let region = self.region_containing_addr(addr, &regions);
        region.write_u32(addr, val)
    }

    fn write_u64(&self, addr: u64, val: u64) {
        todo!()
    }
}

#[derive(Debug)]
pub struct MachineState {
    registers: BTreeMap<Register, Box<CpuRegister>>,
    ram: Ram,
}

impl MachineState {
    pub fn new() -> Self {
        let registers =
            BTreeMap::from_iter(Register::iter().map(|reg| (reg, Box::new(CpuRegister::new(reg)))));

        let ram = Ram::new();
        // 4kb stack
        let stack_top = 0x80000000;
        let stack_size = 0x1000;
        let stack_bottom = stack_top - stack_size;
        ram.add_region(VirtualMemoryRegion::new(stack_bottom, stack_size));

        let ret = Self { registers, ram };

        // Assign the stack pointer to the top of the region we allocated above
        ret.reg_view(&RegView::rsp())
            .write(&ret, stack_top as usize);

        ret
    }

    // TODO(PT): Deprecate in favor of reg_view(), and rename reg_view to reg()
    pub fn reg(&self, reg: Register) -> &dyn VariableStorage {
        &*self.registers[&reg]
    }

    pub fn reg_view(&self, reg: &RegView) -> CpuRegisterView {
        CpuRegisterView::new(reg, &*self.registers[&reg.0])
    }

    fn run_instruction(&self, instr: &Instr) {
        match instr {
            Instr::MoveImmToReg(MoveImmToReg { imm, dest }) => {
                self.reg_view(dest).write(self, *imm)
            }
            Instr::PushFromReg(reg) => {
                let original_rsp = self.reg(Rsp).read_u64(&self);
                let slot = original_rsp - (mem::size_of::<u32>() as u64);
                let value = self.reg(reg.0).read_u32(&self);

                // Write the value to memory
                self.ram.write_u32(slot, value);

                // Decrement the stack pointer
                self.reg(Rsp).write_u64(&self, slot);
            }
            Instr::PopIntoReg(reg) => {
                let original_rsp = self.reg(Rsp).read_u64(&self);
                let slot = original_rsp;
                // Read the value from stack memory
                let value = self.ram.read_u32(slot);
                // Write it to the destination register
                self.reg(reg.0).write_u32(&self, value);
                // Increment the stack pointer
                self.reg(Rsp)
                    .write_u64(&self, original_rsp + (mem::size_of::<u32>() as u64));
            }
            Instr::AddRegToReg(AddRegToReg { augend, addend }) => {
                let augend_val = self.reg_view(augend).read(&self);
                let addend_val = self.reg_view(addend).read(&self);
                // TODO(PT): Handle over/underflow
                self.reg_view(augend).write(&self, augend_val + addend_val);
            }
            Instr::SubRegFromReg(SubRegFromReg {
                minuend,
                subtrahend,
            }) => {
                let minuend_val = self.reg_view(minuend).read(&self);
                let subtrahend_val = self.reg_view(subtrahend).read(&self);
                // TODO(PT): Handle over/underflow
                self.reg_view(minuend)
                    .write(&self, minuend_val - subtrahend_val);
            }
            Instr::MulRegByReg(MulRegByReg {
                multiplicand,
                multiplier,
            }) => {
                let multiplicand_val = self.reg_view(multiplicand).read(&self);
                let multiplier_val = self.reg_view(multiplier).read(&self);
                // TODO(PT): Handle over/underflow
                self.reg_view(multiplicand)
                    .write(&self, multiplicand_val * multiplier_val);
            }
            Instr::DivRegByReg(DivRegByReg { dividend, divisor }) => {
                let dividend_val = self.reg_view(dividend).read(&self);
                let divisor_val = self.reg_view(divisor).read(&self);
                // TODO(PT): Handle over/underflow
                // TODO(PT): Continue here
                //self.reg(*multiplicand).write_u32(&self, multiplicand_val * multiplier_val);
                todo!()
            }
            Instr::DirectiveDeclareGlobalSymbol(_symbol_name) => {
                // Nothing to do at runtime
            }
            Instr::DirectiveDeclareLabel(_label_name) => {
                // Nothing to do at runtime
            }
            Instr::MoveRegToReg(MoveRegToReg { source, dest }) => {
                let source_val = self.reg_view(source).read(&self);
                self.reg_view(dest).write(&self, source_val);
            }
            Instr::Return => {
                // Not handled yet
                self.set_rip(0)
            }
            Instr::CompareImmWithReg(CompareImmWithReg { imm, reg }) => {
                let reg_val = self.reg_view(reg).read(&self);
                let result = (*imm as isize) - (reg_val as isize);
                if result == 0 {
                    self.update_flag(FlagUpdate::Zero(true));
                    self.update_flag(FlagUpdate::Carry(false));
                } else {
                    self.update_flag(FlagUpdate::Zero(false));
                    if result < 0 {
                        self.update_flag(FlagUpdate::Carry(true));
                    } else {
                        self.update_flag(FlagUpdate::Carry(false));
                    }
                }
            }
            Instr::CompareRegWithReg(CompareRegWithReg { reg1, reg2 }) => {
                let reg1_val = self.reg_view(reg1).read(&self);
                let reg2_val = self.reg_view(reg2).read(&self);
                let result = (reg2_val as isize) - (reg1_val as isize);
                if result == 0 {
                    self.update_flag(FlagUpdate::Zero(true));
                    self.update_flag(FlagUpdate::Carry(false));
                } else {
                    self.update_flag(FlagUpdate::Zero(false));
                    if result < 0 {
                        self.update_flag(FlagUpdate::Carry(true));
                    } else {
                        self.update_flag(FlagUpdate::Carry(false));
                    }
                }
            }
            Instr::JumpToLabelIfEqual(label_name) => {
                todo!()
            }
            Instr::JumpToRelOffIfEqual(rel_off) => {
                if self.is_flag_condition_met(FlagCondition::Zero) {
                    // Jump is taken
                    let new_rip = self.get_rip() as isize + rel_off;
                    self.set_rip(new_rip as usize)
                } else {
                    // Jump not taken
                    // Nothing to do
                }
            }
            Instr::JumpToRelOffIfNotEqual(rel_off) => {
                if self.is_flag_condition_met(FlagCondition::NotZero) {
                    // Jump is taken
                    let new_rip = self.get_rip() as isize + rel_off;
                    self.set_rip(new_rip as usize)
                } else {
                    // Jump not taken
                    // Nothing to do
                }
            }
            Instr::SimulatorShimGetInput => {
                print!("\n[Simulator::sim_shim_get_input] Type an int >>> ");
                io::stdout().flush();
                let mut input_text = String::new();
                io::stdin()
                    .read_line(&mut input_text)
                    .expect("failed to read from stdin");

                let trimmed = input_text.trim();
                match trimmed.parse::<u32>() {
                    //Ok(i) => self.reg_view(&RegView::eax()).write(&self, i as usize),
                    Ok(i) => self.reg_view(&RegView::eax()).write(&self, i as usize),
                    Err(..) => panic!("this was not an integer: {}", trimmed),
                };
            }
            _ => {
                println!("Instr not implemented: {instr:?}");
                todo!()
            }
        }
    }

    pub fn run_instructions(&self, instrs: &[Instr]) {
        for instr in instrs.iter() {
            self.run_instruction(instr);
        }
    }

    fn is_flag_condition_met(&self, cond: FlagCondition) -> bool {
        match cond {
            FlagCondition::NoCarry => !self.is_flag_set(Flag::Carry),
            FlagCondition::Carry => self.is_flag_set(Flag::Carry),
            FlagCondition::ParityOdd => !self.is_flag_set(Flag::Parity),
            FlagCondition::ParityEven => self.is_flag_set(Flag::Parity),
            FlagCondition::NoAuxCarry => !self.is_flag_set(Flag::AuxiliaryCarry),
            FlagCondition::AuxCarry => self.is_flag_set(Flag::AuxiliaryCarry),
            FlagCondition::NotZero => !self.is_flag_set(Flag::Zero),
            FlagCondition::Zero => self.is_flag_set(Flag::Zero),
            FlagCondition::SignPositive => !self.is_flag_set(Flag::Sign),
            FlagCondition::SignNegative => self.is_flag_set(Flag::Sign),
            FlagCondition::NotOverflow => !self.is_flag_set(Flag::Overflow),
            FlagCondition::Overflow => self.is_flag_set(Flag::Overflow),
        }
    }

    fn update_flag(&self, flag: FlagUpdate) {
        let flag_setting_and_bit_index = match flag {
            FlagUpdate::Carry(on) => (on, 0),
            FlagUpdate::Parity(on) => (on, 2),
            FlagUpdate::AuxiliaryCarry(on) => (on, 4),
            FlagUpdate::Zero(on) => (on, 6),
            FlagUpdate::Sign(on) => (on, 7),
            FlagUpdate::Overflow(on) => (on, 11),
        };
        let bit_index = flag_setting_and_bit_index.1;
        let flags_reg = self.reg_view(&RegView::rflags());
        let mut flags = flags_reg.read(self);
        if flag_setting_and_bit_index.0 {
            // Enable flag
            flags |= 1 << bit_index;
        } else {
            // Disable flag
            flags &= !(1 << bit_index);
        }
        flags_reg.write(self, flags);
    }

    fn is_flag_set(&self, flag: Flag) -> bool {
        let flag_bit_index = match flag {
            Flag::Carry => 0,
            Flag::Parity => 2,
            Flag::AuxiliaryCarry => 4,
            Flag::Zero => 6,
            Flag::Sign => 7,
            Flag::Overflow => 11,
        };
        let flags = self.reg_view(&RegView::rflags()).read(&self);
        (flags & (1 << flag_bit_index)) != 0
    }

    pub fn get_rip(&self) -> usize {
        self.reg_view(&RegView::rip()).read(&self)
    }

    fn set_rip(&self, new_rip: usize) {
        self.reg_view(&RegView::rip()).write(&self, new_rip)
    }

    fn disassemble_instruction(&self, rip: u64) -> InstrInfo {
        let bytecode_provider = RamBytecodeProvider::new(&self.ram, rip);
        let mut disassembler = InstrDisassembler::new(&bytecode_provider);
        disassembler.disassemble()
    }

    pub fn step(&self) -> InstrInfo {
        let rip = self.get_rip();
        let info = self.disassemble_instruction(rip.try_into().unwrap());
        self.run_instruction(&info.instr);
        // Check whether the instruction jumped or not
        if info.continuation != InstrContinuation::Seq && self.get_rip() != rip {
            // The instruction modified rip so it must have jumped
            // We need to add in the size of the jump instruction itself to the new rip, though
            self.set_rip(self.get_rip() + info.instr_size);
            println!(
                "Detected a jump from instr {info:?}, new RIP {:#x}",
                self.get_rip()
            )
        } else {
            // We need to bump rip ourselves
            self.set_rip(self.get_rip() + info.rip_increment.unwrap())
        }
        info
    }

    pub fn load_elf(&self, elf_bytes: &[u8]) {
        println!("Loading ELF of size {}...", elf_bytes.len());

        let mut cursor = 0;
        let elf_header = unsafe {
            mem::transmute::<[u8; mem::size_of::<ElfHeader64>()], ElfHeader64>(
                elf_bytes[cursor..cursor + mem::size_of::<ElfHeader64>()]
                    .try_into()
                    .unwrap(),
            )
        };
        cursor += mem::size_of::<ElfHeader64>();
        //println!("Got ELF header {elf_header:?}");

        let segments_start = elf_header.program_header_table_start;
        for i in 0..elf_header.program_header_table_entry_count {
            let segment_header = unsafe {
                mem::transmute::<[u8; mem::size_of::<ElfSegment64>()], ElfSegment64>(
                    elf_bytes[cursor..cursor + mem::size_of::<ElfSegment64>()]
                        .try_into()
                        .unwrap(),
                )
            };
            cursor += mem::size_of::<ElfSegment64>();

            //println!("\tSegment header {i}: {segment_header:?}");

            // Map the segment into memory
            let segment_file_data = &elf_bytes[(segment_header.offset as usize)
                ..(segment_header.offset + segment_header.file_size) as usize];
            let mapped_segment =
                VirtualMemoryRegion::new_with_contents(segment_header.vaddr, segment_file_data);
            self.ram.add_region(mapped_segment);
        }

        // Set the entry point to what the ELF designates
        self.reg_view(&RegView::rip())
            .write(&self, elf_header.entry_point as usize);
    }
}

#[derive(Constructor)]
struct RamBytecodeProvider<'a> {
    ram: &'a Ram,
    initial_addr: u64,
}

impl InstrBytecodeProvider for RamBytecodeProvider<'_> {
    fn get_byte(&self, offset: u64) -> u8 {
        self.ram.read_u8(self.initial_addr + offset)
    }
}

#[cfg(test)]
mod test {
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;

    use compilation_definitions::instructions::{
        AddRegToReg, CompareImmWithReg, Instr, MoveImmToReg, MoveRegToReg,
    };
    use compilation_definitions::prelude::*;

    use crate::simulator::{FlagCondition, FlagUpdate, MachineState, VariableStorage};

    fn get_machine() -> MachineState {
        MachineState::new()
    }

    #[test]
    fn test_move_imm_to_reg() {
        // Given a machine
        let machine = get_machine();

        // When I run an instruction to move a u8 constant to rax
        machine.run_instruction(&Instr::MoveImmToReg(MoveImmToReg::new(3, RegView::al())));
        // Then rax contains the expected value
        assert_eq!(machine.reg(Rax).read_u8(&machine), 3);

        // And when the register contains other data in the other bytes
        machine.reg(Rax).write_u64(&machine, 0xffaabb22);
        assert_eq!(machine.reg(Rax).read_u64(&machine), 0xffaabb22);
        // When I run an instruction to move a u8 constant to rax
        machine.run_instruction(&Instr::MoveImmToReg(MoveImmToReg::new(0xcc, RegView::al())));
        // Then only the lower byte is overwritten
        assert_eq!(machine.reg(Rax).read_u64(&machine), 0xffaabbcc);
    }

    #[test]
    fn test_push_reg32() {
        // Given a machine
        let machine = get_machine();
        let original_sp = machine.reg(Rsp).read_u64(&machine);

        // And rax contains some data
        machine.reg(Rax).write_u64(&machine, 0xdeadbeef);

        // When I run an instruction to push a u32 to the stack from a register
        machine.run_instruction(&Instr::PushFromReg(RegView(Rax, AccessType::RX)));

        // Then the memory has been stored
        assert_eq!(machine.ram.read_u32(original_sp - 4), 0xdeadbeef);

        // And the stack pointer has been decremented by 4 bytes
        let new_sp = machine.reg(Rsp).read_u64(&machine);
        assert_eq!(new_sp, original_sp - 4);
    }

    #[test]
    fn test_pop_reg32() {
        // Given a machine
        let machine = get_machine();

        // And there's a word on the stack
        machine.run_instructions(&[
            Instr::MoveImmToReg(MoveImmToReg::new(0xfe, RegView::rax())),
            Instr::PushFromReg(RegView(Rax, AccessType::RX)),
        ]);

        // When I run an instruction to pop a u32 from the stack
        let original_sp = machine.reg(Rsp).read_u64(&machine);
        machine.run_instruction(&Instr::PopIntoReg(RegView(Rbx, AccessType::RX)));

        // Then the value has been popped into rbx
        assert_eq!(machine.reg(Rbx).read_u32(&machine), 0x00fe);

        // And the stack pointer has been incremented by 4 bytes
        let new_sp = machine.reg(Rsp).read_u64(&machine);
        assert_eq!(new_sp, original_sp + 4);

        // And the stack memory is unmodified
        assert_eq!(machine.ram.read_u32(original_sp), 0x00fe);
    }

    #[test]
    fn test_add() {
        // Given a machine
        let machine = get_machine();

        // When I run a simple instruction sequence
        machine.run_instructions(&[
            Instr::MoveImmToReg(MoveImmToReg::new(3, RegView::rax())),
            Instr::PushFromReg(RegView::rax()),
            Instr::MoveImmToReg(MoveImmToReg::new(7, RegView::rax())),
            Instr::PushFromReg(RegView::rax()),
            Instr::PopIntoReg(RegView::rax()),
            Instr::PopIntoReg(RegView::rbx()),
            Instr::AddRegToReg(AddRegToReg::new(RegView::rax(), RegView::rbx())),
            Instr::PushFromReg(RegView::rax()),
            Instr::MoveImmToReg(MoveImmToReg::new(2, RegView::rax())),
            Instr::PushFromReg(RegView::rax()),
            Instr::PopIntoReg(RegView::rax()),
            Instr::PopIntoReg(RegView::rbx()),
            Instr::AddRegToReg(AddRegToReg::new(RegView::rax(), RegView::rbx())),
        ]);

        // Then rax contains the correct computed value
        assert_eq!(machine.reg(Rax).read_u64(&machine), 12);
        println!("machine {machine:?}");
    }

    #[test]
    fn test_move_access_type() {
        // Given a machine
        let machine = get_machine();
        // When I run various move instructions of differing access types
        // Then the correct portion of the source registers are copied
        let src = machine.registers.get(&Rax).unwrap();
        let dst = machine.registers.get(&Rbx).unwrap();
        src.write_u64(&machine, 0xfeed_d00d_dead_beef);

        // Only copy low byte
        machine.run_instruction(&Instr::MoveRegToReg(MoveRegToReg::new(
            RegView(src.reg, AccessType::L),
            RegView(dst.reg, AccessType::L),
        )));
        assert_eq!(dst.read_u64(&machine), 0xef);
        // Clear destination register to set up for next test
        dst.write_u64(&machine, 0x0);

        // Given the destination register contains some data
        dst.write_u64(&machine, 0xaaaa_bbbb_cccc_dddd);
        // When the move is addressed to the high byte in the u16 view
        machine.run_instruction(&Instr::MoveRegToReg(MoveRegToReg::new(
            RegView(src.reg, AccessType::H),
            RegView(dst.reg, AccessType::H),
        )));
        // Then all the bytes are untouched except for the high byte of the low u16
        assert_eq!(dst.read_u64(&machine), 0xaaaa_bbbb_cccc_bedd);
        // Clear destination register to set up for next test
        dst.write_u64(&machine, 0x0);

        // Given the high 48 bits contains some data
        dst.write_u64(&machine, 0xc0de_dead_cafe_babe);
        // When the move is addressed to the low u16
        machine.run_instruction(&Instr::MoveRegToReg(MoveRegToReg::new(
            RegView(src.reg, AccessType::X),
            RegView(dst.reg, AccessType::X),
        )));
        // Then the high 48 bits are untouched, and the lower u16 is overwritten
        assert_eq!(dst.read_u64(&machine), 0xc0de_dead_cafe_beef);
        // Clear destination register to set up for next test
        dst.write_u64(&machine, 0x0);

        // Given the high u32 contains some data
        dst.write_u64(&machine, 0xc0de_dead_cafe_babe);
        // When the move is addressed to the low u32
        machine.run_instruction(&Instr::MoveRegToReg(MoveRegToReg::new(
            RegView(src.reg, AccessType::EX),
            RegView(dst.reg, AccessType::EX),
        )));
        // Then the high u32 is untouched, and the lower u32 is overwritten
        assert_eq!(dst.read_u64(&machine), 0xc0de_dead_dead_beef);
        // Clear destination register to set up for next test
        dst.write_u64(&machine, 0x0);

        // Given the destination register contains a u64
        dst.write_u64(&machine, 0xc0de_dead_cafe_babe);
        // When the move is addressed to the full u64
        machine.run_instruction(&Instr::MoveRegToReg(MoveRegToReg::new(
            RegView(src.reg, AccessType::RX),
            RegView(dst.reg, AccessType::RX),
        )));
        // Then the full register is overwritten
        assert_eq!(dst.read_u64(&machine), 0xfeed_d00d_dead_beef);
    }

    #[test]
    fn test_compare_imm_with_reg() {
        // Given an instruction to compare an immediate with a register
        let machine = get_machine();
        let test_cases = [
            (5, 8, vec![FlagCondition::NotZero, FlagCondition::Carry]),
            (8, 5, vec![FlagCondition::NotZero, FlagCondition::NoCarry]),
            (5, 5, vec![FlagCondition::Zero, FlagCondition::NoCarry]),
        ];
        for (imm_val, reg_val, expected_flags) in test_cases {
            // Clear flags from the last run
            machine.update_flag(FlagUpdate::Zero(false));
            machine.update_flag(FlagUpdate::Carry(false));

            // And a register contains the provided value
            let reg = RegView::rax();
            machine.reg_view(&reg).write(&machine, reg_val);

            // When the instruction is run
            machine.run_instruction(&Instr::CompareImmWithReg(CompareImmWithReg::new(
                imm_val, reg,
            )));

            // Then the flags contain the expected values
            for expected_flag in expected_flags.iter() {
                assert!(
                    machine.is_flag_condition_met(*expected_flag),
                    "Expected {expected_flag:?} with {imm_val} - {reg_val:?}"
                )
            }
        }
    }
}
