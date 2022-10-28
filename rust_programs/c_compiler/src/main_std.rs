use std::error;
use std::io::{self, BufRead, Write};
use std::rc::Rc;
use std::env;
use std::fs;

use linker::{FileLayout, assembly_packer, render_elf};
use compilation_definitions::instructions::{Instr, MoveImmToReg, MoveRegToReg, AddRegToReg, CompareImmWithReg};
use compilation_definitions::prelude::*;

use crate::codegen::CodeGenerator;
use crate::optimizer::Optimizer;
use crate::parser::Parser;
use crate::simulator::MachineState;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let source = "int main() { \
    if (sim_shim_get_input() == 4) {
        return 5;
    }
    return 10;
    }";

    // Parse the source code to an AST
    println!("Parsing source code...");
    let mut parser = Parser::new(&source);
    let func = parser.parse_function();

    // Generate IR
    println!("Generating IR...");
    let codegen = CodeGenerator::new();
    let instrs = codegen.codegen_function(&func);

    // Optimize IR
    println!("Optimizing IR...");
    let mut optimized_instrs = Optimizer::optimize(&instrs);
    optimized_instrs.insert(0, Instr::DirectiveSetCurrentSection(".text".to_string()));

    // Render IR to assembly
    println!("Rendering IR to assembly...");
    let instrs_as_asm = CodeGenerator::render_instructions_to_assembly(&optimized_instrs);
    println!("Rendered assembly: ");
    for asm_instr in instrs_as_asm.iter() {
        println!("\t{asm_instr}");
    }
    let mut asm_source = instrs_as_asm.join("\n");
    // TODO(PT): Newline at end is to deal with a bug in assembler lexer
    asm_source.push('\n');

    // Assemble into an ELF
    println!("Assembling to an ELF...");
    let layout = Rc::new(FileLayout::new(0x400000));
    let (labels, equ_expressions, atoms) = assembly_packer::parse(&layout, &asm_source);
    let elf = render_elf(&layout, labels, equ_expressions, atoms);
    println!("Finshed ELF generation. Size: {}\n", elf.len());

    let current_dir = env::current_dir().unwrap();
    let output_file = current_dir.join("output_elf");
    println!("Output file {output_file:?}");
    fs::write(output_file, elf.clone()).unwrap();

    // Simulate ELF execution
    println!("Simulating ELF...");
    let machine = MachineState::new();
    machine.load_elf(&elf);
    loop {
        let rip = machine.get_rip();
        let instr_info = machine.step();
        if let Instr::Return = instr_info.instr {
            break;
        }
        println!("{rip:#x}: {instr_info}");
    }
    println!("Simulation complete!");
    println!("rax = {}", machine.reg(Rax).read_u64(&machine));

    Ok(())
}

#[cfg(test)]
mod test {
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;

    use compilation_definitions::instructions::{AddRegToReg, Instr, MoveImmToReg, MoveRegToReg, SubRegFromReg, MulRegByReg, DivRegByReg, CompareImmWithReg};
    use compilation_definitions::prelude::*;
    use linker::{FileLayout, assembly_packer, render_elf};

    use crate::codegen::CodeGenerator;
    use crate::parser::{Parser, InfixOperator, Expr};
    use crate::optimizer::Optimizer;
    use crate::simulator::MachineState;

    // Integration tests

    fn codegen_and_execute_source(source: &str) -> (Vec<Instr>, MachineState) {
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);
        let optimized_instrs = Optimizer::optimize(&instrs);
        let instrs_as_asm = CodeGenerator::render_instructions_to_assembly(&optimized_instrs);
        let mut asm_source = instrs_as_asm.join("\n");
        // TODO(PT): Newline at end is to deal with a bug in assembler lexer
        asm_source.push('\n');

        // Assemble into an ELF
        let layout = Rc::new(FileLayout::new(0x400000));
        let (labels, equ_expressions, atoms) = assembly_packer::parse(&layout, &asm_source);
        let elf = render_elf(&layout, labels, equ_expressions, atoms);

        // Simulate ELF execution
        let machine = MachineState::new();
        machine.load_elf(&elf);
        loop {
            let instr_info = machine.step();
            if let Instr::Return = instr_info.instr {
                break;
            }
        }
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
                Instr::DirectiveDeclareGlobalSymbol("_foo".to_string()),
                // Function entry point
                Instr::DirectiveDeclareLabel("_foo".to_string()),
                // Set up stack frame
                Instr::PushFromReg(RegView::rbp()),
                Instr::MoveRegToReg(MoveRegToReg::new(RegView::rsp(), RegView::rbp())),
                // If test
                Instr::MoveImmToReg(MoveImmToReg::new(1, RegView::rax())),
                Instr::CompareImmWithReg(CompareImmWithReg::new(0, RegView::eax())),
                // Conditional jump past consequent
                Instr::JumpToLabelIfEqual("_L0_if_test_failed".to_string()),
                // Consequent
                // Return 3
                Instr::MoveImmToReg(MoveImmToReg::new(3, RegView::rax())),
                Instr::PopIntoReg(RegView::rbp()),
                Instr::Return,
                // Jump past alternate
                Instr::JumpToLabel("_L1_if_statement_finished".to_string()),
                // Alternate
                Instr::DirectiveDeclareLabel("_L0_if_test_failed".to_string()),
                Instr::DirectiveDeclareLabel("_L1_if_statement_finished".to_string()),
                // Return 5
                Instr::MoveImmToReg(MoveImmToReg::new(5, RegView::rax())),
                Instr::PopIntoReg(RegView::rbp()),
                Instr::Return,
            ]
        );

        // And when I emulate the instructions
        // Then rax contains the correct value
        assert_eq!(machine.reg(Rax).read_u32(&machine), 3);
    }

}
