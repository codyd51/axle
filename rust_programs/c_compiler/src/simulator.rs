use crate::instructions::{AddReg32ToReg32, DivReg32ByReg32, Instr, MoveImm32ToReg32, MoveImm8ToReg8, MoveReg8ToReg8, MulReg32ByReg32, SubReg32FromReg32};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::{format, vec};
use alloc::string::String;
use alloc::vec::Vec;
use core::cell::RefCell;
use core::fmt::{Debug, Display, Formatter};
use core::mem;
use strum::IntoEnumIterator;

use crate::prelude::*;

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
        todo!()
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
        todo!()
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
struct Ram {
    store: RefCell<Vec<u8>>,
}

impl Ram {
    fn new() -> Self {
        Self {
            // PT: We might need to switch to a more robust virtual memory implementation?
            // We might also need to bump the memory allocated here
            store: RefCell::new(vec![0; 0x1000]),
        }
    }

    fn read_u8(&self, addr: u64) -> u8 {
        todo!()
    }

    fn read_u16(&self, addr: u64) -> u16 {
        todo!()
    }

    fn read_u32(&self, addr: u64) -> u32 {
        let store = self.store.borrow();
        let bytes = clone_into_array(&store[(addr as _) .. (addr as usize + mem::size_of::<u32>())]);
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

    fn write_u32(&self, addr: u64, val: u32) {
        let mut store = self.store.borrow_mut();
        for (i, b) in val.to_ne_bytes().iter().enumerate() {
            store[(addr as usize) + i] = *b;
        }
    }

    fn write_u64(&self, addr: u64, val: u64) {
        todo!()
    }
}

#[derive(Debug)]
pub struct MachineState {
    registers: BTreeMap<Register, Box<dyn VariableStorage>>,
    ram: Ram,
}


impl MachineState {
    pub fn new() -> Self {
        let registers = BTreeMap::from_iter(
            Register::iter()
                .map(|reg| (reg, Box::new(CpuRegister::new(reg)) as Box<dyn VariableStorage>))
        );

        let ret = Self {
            registers,
            ram: Ram::new()
        };

        // Set the stack pointer to the top of available memory
        ret.reg(Rsp).write_u64(&ret, ret.ram.store.borrow().len() as u64);

        ret
    }

    pub fn reg(&self, reg: Register) -> &dyn VariableStorage {
        &*self.registers[&reg]
    }

    fn run_instruction(&self, instr: &Instr) {
        match instr {
            Instr::MoveImm8ToReg8(MoveImm8ToReg8 { imm, dest }) => {
                self.reg(*dest).write_u8(self, *imm as u8)
            },
            Instr::MoveImm32ToReg32(MoveImm32ToReg32 { imm, dest }) => {
                self.reg(*dest).write_u32(self, *imm as u32)
            },
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
                self.reg(Rsp).write_u64(&self, original_rsp + (mem::size_of::<u32>() as u64));
            }
            Instr::AddReg8ToReg8(AddReg32ToReg32 { augend, addend }) => {
                let augend_val = self.reg(*augend).read_u32(&self);
                let addend_val = self.reg(*addend).read_u32(&self);
                // TODO(PT): Handle over/underflow
                self.reg(*augend).write_u32(&self, augend_val + addend_val);
            }
            Instr::SubReg32FromReg32(SubReg32FromReg32 { minuend, subtrahend }) => {
                let minuend_val = self.reg(*minuend).read_u32(&self);
                let subtrahend_val = self.reg(*subtrahend).read_u32(&self);
                // TODO(PT): Handle over/underflow
                self.reg(*minuend).write_u32(&self, minuend_val - subtrahend_val);
            }
            Instr::MulReg32ByReg32(MulReg32ByReg32 { multiplicand, multiplier }) => {
                let multiplicand_val = self.reg(*multiplicand).read_u32(&self);
                let multiplier_val = self.reg(*multiplier).read_u32(&self);
                // TODO(PT): Handle over/underflow
                self.reg(*multiplicand).write_u32(&self, multiplicand_val * multiplier_val);
            }
            Instr::DivReg32ByReg32(DivReg32ByReg32 { dividend, divisor }) => {
                let dividend_val = self.reg(*dividend).read_u32(&self);
                let divisor_val = self.reg(*divisor).read_u32(&self);
                // TODO(PT): Handle over/underflow
                // TODO(PT): Continue here
                //self.reg(*multiplicand).write_u32(&self, multiplicand_val * multiplier_val);
            }
            Instr::DirectiveDeclareGlobalSymbol(_symbol_name) => {
                // Nothing to do at runtime
            }
            Instr::DirectiveDeclareLabel(_label_name) => {
                // Nothing to do at runtime
            }
            Instr::MoveReg8ToReg8(MoveReg8ToReg8 { source, dest }) => {
                let source_val = self.reg(*source).read_u32(&self);
                self.reg(*dest).write_u32(&self, source_val);
            }
            Instr::Return => {
                // Not handled yet
            }
            _ => {
                println!("Instr not implemented: {instr:?}");
                todo!()
            },
        }
    }

    pub fn run_instructions(&self, instrs: &[Instr]) {
        for instr in instrs.iter() {
            self.run_instruction(instr);
        }
    }
}

#[cfg(test)]
mod test {
    use crate::simulator::MachineState;
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;
    use crate::instructions::{AddReg32ToReg32, Instr, MoveImm8ToReg8};
    use crate::prelude::*;

    fn get_machine() -> MachineState {
        MachineState::new()
    }

    #[test]
    fn test_move_imm8_to_reg8() {
        // Given a machine
        let machine = get_machine();

        // When I run an instruction to move a u8 constant to rax
        machine.run_instruction(
            &Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(3, Rax)),
        );
        // Then rax contains the expected value
        assert_eq!(machine.reg(Rax).read_u8(&machine), 3);

        // And when the register contains other data in the other bytes
        machine.reg(Rax).write_u64(&machine, 0xffaabb22);
        assert_eq!(machine.reg(Rax).read_u64(&machine), 0xffaabb22);
        // When I run an instruction to move a u8 constant to rax
        machine.run_instruction(
            &Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(0xcc, Rax)),
        );
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
        machine.run_instruction(
            &Instr::PushFromReg(RegisterView(Rax, AccessType::RX))
        );

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
        machine.run_instructions(
            &[
                Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(0xfe, Rax)),
                Instr::PushFromReg(RegisterView(Rax, AccessType::RX))
            ]
        );

        // When I run an instruction to pop a u32 from the stack
        let original_sp = machine.reg(Rsp).read_u64(&machine);
        machine.run_instruction(
            &Instr::PopIntoReg(RegisterView(Rbx, AccessType::RX))
        );

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
        machine.run_instructions(
            &[
                Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(3, Rax)),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(7, Rax)),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddReg8ToReg8(AddReg32ToReg32::new(Rax, Rbx)),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImm8ToReg8(MoveImm8ToReg8::new(2, Rax)),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddReg8ToReg8(AddReg32ToReg32::new(Rax, Rbx))
            ]
        );

        // Then rax contains the correct computed value
        assert_eq!(machine.reg(Rax).read_u64(&machine), 12);
        println!("machine {machine:?}");
    }
}
