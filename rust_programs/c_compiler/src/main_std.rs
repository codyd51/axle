use std::error;
use std::io::{self, BufRead, Write};

use crate::codegen::CodeGenerator;
use crate::parser::Parser;
use crate::simulator::MachineState;
use crate::prelude::*;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Starting REPL...");

    let stdin = io::stdin();
    let mut stdin_iter = stdin.lock().lines();

    loop {
        print!(">>> ");
        io::stdout().flush();
        let source = stdin_iter.next().unwrap().unwrap();
        let mut parser = Parser::new(&source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);
        let machine = MachineState::new();
        machine.run_instructions(&instrs);
        println!("rax = {}", machine.reg(Rax).read_u64(&machine));
    }

    Ok(())
}

#[cfg(test)]
mod test {
    use crate::simulator::MachineState;
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;
    use crate::instructions::{AddReg32ToReg32, Instr, MoveImmToReg, MoveRegToReg, SubReg32FromReg32, MulReg32ByReg32, DivReg32ByReg32};
    use crate::codegen::CodeGenerator;
    use crate::parser::{Parser, InfixOperator, Expr};
    use crate::prelude::*;

    // Integration tests

    fn codegen_and_execute_source(source: &str) -> (Vec<Instr>, MachineState) {
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
                Instr::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instr::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instr::PushFromReg(RegisterView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegisterView::rsp(), RegisterView::rbp())),
                // Compute parenthesized expression
                Instr::MoveImmToReg(MoveImmToReg::new(3, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(7, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddReg8ToReg8(AddReg32ToReg32::new(Rax, Rbx)),
                // Compute second expression
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(2, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddReg8ToReg8(AddReg32ToReg32::new(Rax, Rbx)),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegisterView::rbp()),
                Instr::Return
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 12);
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
                Instr::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instr::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instr::PushFromReg(RegisterView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegisterView::rsp(), RegisterView::rbp())),
                // Compute subtraction
                Instr::MoveImmToReg(MoveImmToReg::new(100, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(66, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::SubReg32FromReg32(SubReg32FromReg32::new(Rax, Rbx)),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegisterView::rbp()),
                Instr::Return
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 34);
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
                Instr::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instr::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instr::PushFromReg(RegisterView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegisterView::rsp(), RegisterView::rbp())),
                // Compute multiplication
                Instr::MoveImmToReg(MoveImmToReg::new(300, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(18, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::MulReg32ByReg32(MulReg32ByReg32::new(Rax, Rbx)),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegisterView::rbp()),
                Instr::Return
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 5400);
    }
}
