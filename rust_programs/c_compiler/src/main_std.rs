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
    use crate::emulator::MachineState;
    use alloc::rc::Rc;
    use alloc::vec;
    use core::cell::RefCell;
    use crate::codegen::{CodeGenerator, Instruction, MoveImm8ToReg8, MoveReg8ToReg8, Register};
    use crate::parser::{Parser, InfixOperator, Expr};

    // Integration tests

    #[test]
    fn test_binary_add() {
        // Given a function that returns a binary add expression
        let source = "void foo() { return (20 + 100) + 24; }";
        // When I parse and codegen it
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        let instrs = codegen.codegen_function(&func);

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
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(3, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(7, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(Register::Rax, Register::Rbx),
                // Compute second expression
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(2, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(Register::Rax, Register::Rbx),
                // Clean up stack frame and return
                Instruction::PopIntoReg32(Register::Rbp),
                Instruction::Return8
            ]
        );

        // And when I emulate the instructions
        let machine = MachineState::new();
        machine.run_instructions(&instrs);
        // Then rax contains the correct value
        assert_eq!(machine.reg(Register::Rax).read_u32(&machine), 12);
    }
}
