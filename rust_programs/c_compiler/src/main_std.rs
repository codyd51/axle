use std::error;
use std::io::{self, BufRead, Write};
use std::rc::Rc;
use std::env;
use std::fs;

use linker::{FileLayout, assembly_packer, render_elf};
use compilation_definitions::instructions::Instr;
use compilation_definitions::prelude::*;

use crate::codegen::CodeGenerator;
use crate::optimizer::Optimizer;
use crate::parser::Parser;
use crate::simulator::MachineState;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Starting REPL...");

    let stdin = io::stdin();
    let mut stdin_iter = stdin.lock().lines();

    //loop {
        print!(">>> ");
        io::stdout().flush();
        //let source = stdin_iter.next().unwrap().unwrap();
        let source = "int main() { return 100 + 122 + 458 + 1; }";
        let mut parser = Parser::new(&source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);
        let mut optimized_instrs = Optimizer::optimize(&instrs);
        optimized_instrs.insert(0, Instr::DirectiveSetCurrentSection(".text".to_string()));
        let instrs_as_asm = CodeGenerator::render_instructions_to_assembly(&optimized_instrs);
        println!("Codegen instructions: ");
        for instr in optimized_instrs.iter() {
            println!("\t{instr:?}");
        }
        println!("Rendered assembly: ");
        for asm_instr in instrs_as_asm.iter() {
            println!("\t{asm_instr}");
        }
        let mut asm_source = instrs_as_asm.join("\n");
        // TODO(PT): Newline at end is to deal with a bug in assembler lexer
        asm_source.push('\n');
        let layout = Rc::new(FileLayout::new(0x400000));
        let (labels, equ_expressions, atoms) = assembly_packer::parse(&layout, &asm_source);
        let elf = render_elf(&layout, labels, equ_expressions, atoms);

        println!("Finshed ELF generation. Size: {}\n", elf.len());
        let current_dir = env::current_dir().unwrap();
        let output_file = current_dir.join("output_elf");
        //fs::write(output_file, elf).unwrap();

        let machine = MachineState::new();
        machine.load_elf(&elf);
        //println!("machine {machine:?}");
        loop {
            let instr_info = machine.step();
            if instr_info.did_return {
                break;
            }
            //println!("Ran instruction {instr_info:?}");
        }
        println!("rax = {}", machine.reg(Rax).read_u64(&machine));

        /*
        let machine = MachineState::new();
        machine.run_instructions(&optimized_instrs);
        */
        //}

    Ok(())
}

#[cfg(test)]
mod test {
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;

    use compilation_definitions::instructions::{AddRegToReg, Instr, MoveImmToReg, MoveRegToReg, SubRegFromReg, MulRegByReg, DivRegByReg};
    use compilation_definitions::prelude::*;

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
