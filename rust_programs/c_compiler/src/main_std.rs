use crate::codegen::CodeGenerator;
use crate::parser::Parser;
use std::error;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Running with std");

    // TODO(PT): Could this be a REPL?
    let source = r"
        int _start() {
            int a = 5;
            a = a + 3;
            return a;
        }";
    let mut parser = Parser::new(source);
    let func = parser.parse_function();
    println!("func: {func:?}");
    let codegen = CodeGenerator::new();
    codegen.generate();

    Ok(())
}

#[cfg(test)]
mod test {
    use crate::simulator::MachineState;
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;
    use crate::codegen::{AddReg32ToReg32, CodeGenerator, Instruction, MoveImm8ToReg8, MoveImm32ToReg32, MoveReg8ToReg8, Register, SubReg32FromReg32, MulReg32ByReg32, DivReg32ByReg32};
    use crate::parser::{Parser, InfixOperator, Expr};

    // Integration tests

    fn codegen_and_execute_source(source: &str) -> (Vec<Instruction>, MachineState) {
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);
        let machine = MachineState::new();
        machine.run_instructions(&instrs);
        (instrs, machine)
    }

    #[test]
    fn test_binary_add() {
        // Given a function that returns a binary add expression
        // When I parse and codegen it
        let (instrs, machine) = codegen_and_execute_source("void foo() { return (3 + 7) + 2; }");

        // Then the rendered instructions are correct
        assert_eq!(
            instrs,
            vec![
                // Declare the symbol
                Instruction::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instruction::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instruction::PushFromReg32(Register::Rbp),
                Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(Register::Rsp, Register::Rbp)),
                // Compute parenthesized expression
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(3, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(7, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx)),
                // Compute second expression
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(2, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx)),
                // Clean up stack frame and return
                Instruction::PopIntoReg32(Register::Rbp),
                Instruction::Return8
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Register::Rax).read_u32(&machine), 12);
    }

    #[test]
    fn test_binary_sub() {
        // Given a function that returns a binary subtract expression
        // When I parse and codegen it
        let (instrs, machine) = codegen_and_execute_source("void foo() { return 100 - 66; }");

        // Then the rendered instructions are correct
        assert_eq!(
            instrs,
            vec![
                // Declare the symbol
                Instruction::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instruction::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instruction::PushFromReg32(Register::Rbp),
                Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(Register::Rsp, Register::Rbp)),
                // Compute subtraction
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(100, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(66, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::SubReg32FromReg32(SubReg32FromReg32::new(Register::Rax, Register::Rbx)),
                // Clean up stack frame and return
                Instruction::PopIntoReg32(Register::Rbp),
                Instruction::Return8
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Register::Rax).read_u32(&machine), 34);
    }

    #[test]
    fn test_binary_mul() {
        // Given a function that returns a binary multiply expression
        // When I parse and codegen it
        let (instrs, machine) = codegen_and_execute_source("void foo() { return 300 * 18; }");

        // Then the rendered instructions are correct
        assert_eq!(
            instrs,
            vec![
                // Declare the symbol
                Instruction::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instruction::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instruction::PushFromReg32(Register::Rbp),
                Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(Register::Rsp, Register::Rbp)),
                // Compute multiplication
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(300, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(18, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::MulReg32ByReg32(MulReg32ByReg32::new(Register::Rax, Register::Rbx)),
                // Clean up stack frame and return
                Instruction::PopIntoReg32(Register::Rbp),
                Instruction::Return8
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Register::Rax).read_u32(&machine), 5400);
    }
}
