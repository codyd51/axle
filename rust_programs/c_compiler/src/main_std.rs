use std::error;
use std::io::{self, BufRead, Write};
use std::rc::Rc;

use linker::{FileLayout, assembly_packer, render_elf};

use crate::codegen::CodeGenerator;
use crate::optimizer::Optimizer;
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
        let optimized_instrs = Optimizer::optimize(&instrs);
        let machine = MachineState::new();
        machine.run_instructions(&optimized_instrs);
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
    use crate::instructions::{AddRegToReg, Instr, MoveImmToReg, MoveRegToReg, SubRegFromReg, MulRegByReg, DivRegByReg};
    use crate::codegen::CodeGenerator;
    use crate::parser::{Parser, InfixOperator, Expr};
    use crate::optimizer::Optimizer;
    use crate::prelude::*;

    // Integration tests

    fn codegen_and_execute_source(source: &str) -> (Vec<Instr>, MachineState) {
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);
        let optimized_instrs = Optimizer::optimize(&instrs);
        let machine = MachineState::new();
        machine.run_instructions(&optimized_instrs);
        (optimized_instrs, machine)
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
                Instr::PushFromReg(RegView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rsp(), RegView::rbp())),
                // Compute parenthesized expression
                Instr::MoveImmToReg(MoveImmToReg::new(3, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(7, RegView::rax())),
                Instr::PopIntoReg(RegView::rbx()),
                Instr::AddRegToReg(AddRegToReg::new(RegView::rax(), RegView::rbx())),
                // Compute second expression
                Instr::PushFromReg(RegView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(2, RegView::rax())),
                Instr::PopIntoReg(RegView::rbx()),
                Instr::AddRegToReg(AddRegToReg::new(RegView::rax(), RegView::rbx())),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegView::rbp()),
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
                Instr::PushFromReg(RegView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rsp(), RegView::rbp())),
                // Compute subtraction
                Instr::MoveImmToReg(MoveImmToReg::new(100, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(66, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::PopIntoReg(RegView::rbx()),
                Instr::PopIntoReg(RegView::rax()),
                Instr::SubRegFromReg(SubRegFromReg::new(RegView::rax(), RegView::rbx())),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegView::rbp()),
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
                Instr::PushFromReg(RegView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rsp(), RegView::rbp())),
                // Compute multiplication
                Instr::MoveImmToReg(MoveImmToReg::new(300, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(18, RegView::rax())),
                Instr::PopIntoReg(RegView::rbx()),
                Instr::MulRegByReg(MulRegByReg::new(RegView::rax(), RegView::rbx())),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegView::rbp()),
                Instr::Return
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 5400);
    }

    #[test]
    fn test_if() {
        // Given a function that returns a binary subtract expression
        // When I parse and codegen it
        //let (instrs, machine) = codegen_and_execute_source("void foo() { if (1 == 2) { return 3; } return 5; }");
        let (instrs, machine) = codegen_and_execute_source("void foo() { if (1) { return 3; } return 5; }");

        // Then the rendered instructions are correct
        assert_eq!(
            instrs,
            vec![
                // Declare the symbol
                Instr::DirectiveDeclareGlobalSymbol("_foo".into()),
                // Function entry point
                Instr::DirectiveDeclareLabel("_foo".into()),
                // Set up stack frame
                Instr::PushFromReg(RegView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rsp(), RegView::rbp())),
                // Compute subtraction
                Instr::MoveImmToReg(MoveImmToReg::new(100, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(66, RegView::rax())),
                Instr::PushFromReg(RegView::rax()),
                Instr::PopIntoReg(RegView::rbx()),
                Instr::PopIntoReg(RegView::rax()),
                Instr::SubRegFromReg(SubRegFromReg::new(RegView::rax(), RegView::rbx())),
                // Clean up stack frame and return
                Instr::PopIntoReg(RegView::rbp()),
                Instr::Return
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 34);
    }

}
