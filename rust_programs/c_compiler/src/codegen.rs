use crate::lexer::{Lexer, Token};
use crate::parser::{Function, IfStatement, Parser, ReturnStatement, Statement};
use alloc::collections::BTreeMap;
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use alloc::fmt::format;

use crate::println;

#[derive(Debug)]
enum Register {
    Rax,
    Rbp,
    Rsp,
}

impl Register {
    fn asm_name(&self) -> &'static str {
        match self {
            Register::Rax => "rax",
            Register::Rbp => "rbp",
            Register::Rsp => "rsp",
        }
    }
}

#[derive(Debug)]
struct MoveReg8ToReg8 {
    source: Register,
    dest: Register,
}

impl MoveReg8ToReg8 {
    fn new(source: Register, dest: Register) -> Self {
        Self {
            source,
            dest
        }
    }
}

#[derive(Debug)]
struct MoveImm8ToReg8 {
    imm: usize,
    dest: Register,
}

impl MoveImm8ToReg8 {
    fn new(imm: usize, dest: Register) -> Self {
        Self {
            imm,
            dest
        }
    }
}

#[derive(Debug)]
enum Instruction {
    Return8,
    PushFromReg8(Register),
    PopIntoReg8(Register),
    MoveReg8ToReg8(MoveReg8ToReg8),
    MoveImm8ToReg8(MoveImm8ToReg8),
    DirectiveDeclareLabel(String),
}

impl Instruction {
    fn render(&self) -> String {
        match self {
            Instruction::Return8 => "ret".into(),
            Instruction::PushFromReg8(src) => format!("push %{}", src.asm_name()),
            Instruction::PopIntoReg8(dst) => format!("pop %{}", dst.asm_name()),
            Instruction::MoveReg8ToReg8(MoveReg8ToReg8 { source, dest }) => format!("mov %{}, %{}", source.asm_name(), dest.asm_name()),
            Instruction::MoveImm8ToReg8(MoveImm8ToReg8 { imm, dest }) => format!("mov ${imm}, %{}", dest.asm_name()),
            Instruction::DirectiveDeclareLabel(label_name) => format!("{label_name}:"),
        }
    }
}

pub struct CodeGenerator {
    function: Function,
}

impl CodeGenerator {
    pub fn new(function: Function) -> Self {
        Self { function }
    }

    fn render_statement_to_instructions(statement: &Statement) -> Vec<Instruction> {
        let mut statement_instrs = vec![];
        match statement {
            Statement::Return(ReturnStatement { return_value }) => {
                if let Token::Int(imm) = return_value {
                    // Move the immediate return value into rax
                    statement_instrs.push(Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(*imm, Register::Rax)));
                    // Restore the caller's frame pointer
                    statement_instrs.push(Instruction::PopIntoReg8(Register::Rbp));
                    // Return to caller
                    statement_instrs.push(Instruction::Return8);
                }
                else {
                    todo!("Unhandled return type");
                }
            }
            Statement::If(if_statement) => {
                todo!();
            }
            Statement::Block(block_statement) => {
                todo!();
            }
        }
        statement_instrs
    }

    fn render_function_to_instructions(function: &Function) -> Vec<Instruction> {
        let mut func_instrs = vec![];
        // Mangle the function name with a leading underscore
        let mangled_function_name = format!("_{}", function.name);
        // Declare a label denoting the start of the function
        func_instrs.push(Instruction::DirectiveDeclareLabel(mangled_function_name));
        // Save the caller's frame pointer
        func_instrs.push(Instruction::PushFromReg8(Register::Rbp));
        // Set up a new stack frame by storing the starting/base stack pointer in the base pointer
        func_instrs.push(Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(Register::Rsp, Register::Rbp)));

        // Visit each statement in the function
        for statement in function.body.statements.iter() {
            println!("Visting statement {statement:?}");
            let mut statement_instrs = Self::render_statement_to_instructions(&statement);
            func_instrs.append(&mut statement_instrs);
        }

        func_instrs
    }

    fn render_instructions_to_assembly(instructions: Vec<Instruction>) -> Vec<String> {
        let mut assembly = vec![];
        for instr in instructions.iter() {
            assembly.push(instr.render());
        }
        assembly
    }

    pub fn generate(&self) -> Vec<String> {
        let func = &self.function;
        let func_instrs = Self::render_function_to_instructions(func);
        println!("{:?} {}() {{", func.return_type, func.name);
        for instr in func_instrs.iter() {
            println!("\t{instr:?}");
        }
        println!("}}");

        println!("Rendered to assembly\n\n");

        let rendered_instrs = Self::render_instructions_to_assembly(func_instrs);
        println!("{:?} {}() {{", func.return_type, func.name);
        for instr in rendered_instrs.iter() {
            println!("\t{instr}");
        }
        println!("}}");
        rendered_instrs
    }
}

#[cfg(test)]
mod test {
    use alloc::vec;
    use crate::codegen::CodeGenerator;
    use crate::parser::Parser;

    #[test]
    fn test_return_int() {
        let source = r"
        int _start() {
            return 5;
        }";
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new(func);
        let instructions = codegen.generate();
        assert_eq!(
            codegen.generate(),
            vec![
                "__start:",
                "push %rbp",
                "mov %rsp, %rbp",
                "mov $5, %rax",
                "pop %rbp",
                "ret",
            ]
        );
    }
}
