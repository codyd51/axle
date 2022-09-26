use crate::parser::{Expr, Function, ReturnStatement, Statement};
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use core::mem;

use crate::println;

#[derive(Debug, PartialEq)]
enum Register {
    Rax,
    Rdx,
    Rbp,
    Rsp,
}

impl Register {
    fn asm_name(&self) -> &'static str {
        match self {
            Register::Rax => "rax",
            Register::Rdx => "rdx",
            Register::Rbp => "rbp",
            Register::Rsp => "rsp",
        }
    }
}

#[derive(Debug, PartialEq)]
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

#[derive(Debug, PartialEq)]
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

#[derive(Debug, PartialEq)]
struct MoveImm8ToReg8MemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: Register,
}

impl MoveImm8ToReg8MemOffset {
    fn new(imm: usize, offset: isize, reg_to_deref: Register) -> Self {
        Self {
            imm,
            offset,
            reg_to_deref,
        }
    }
}

#[derive(Debug, PartialEq)]
enum Instruction {
    Return8,
    PushFromReg8(Register),
    PopIntoReg8(Register),
    MoveReg8ToReg8(MoveReg8ToReg8),
    MoveImm8ToReg8(MoveImm8ToReg8),
    DirectiveDeclareGlobalSymbol(String),
    DirectiveDeclareLabel(String),
    MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset),
    NegateRegister(Register),
    AddReg8ToReg8(Register, Register),
}

impl Instruction {
    fn render(&self) -> String {
        match self {
            Instruction::Return8 => "ret".into(),
            Instruction::PushFromReg8(src) => format!("push %{}", src.asm_name()),
            Instruction::PopIntoReg8(dst) => format!("pop %{}", dst.asm_name()),
            Instruction::MoveReg8ToReg8(MoveReg8ToReg8 { source, dest }) => format!("mov %{}, %{}", source.asm_name(), dest.asm_name()),
            Instruction::MoveImm8ToReg8(MoveImm8ToReg8 { imm, dest }) => format!("mov ${imm}, %{}", dest.asm_name()),
            Instruction::DirectiveDeclareGlobalSymbol(symbol_name) => format!(".global {symbol_name}"),
            Instruction::DirectiveDeclareLabel(label_name) => format!("{label_name}:"),
            Instruction::NegateRegister(reg) => format!("neg %{}", reg.asm_name()),
            Instruction::AddReg8ToReg8(dst, src) => format!("add %{}, %{}", dst.asm_name(), src.asm_name()),
            _ => todo!(),
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

    fn render_expression_to_instructions(expr: &Expr) -> Vec<Instruction> {
        /*
        let mut expr_instructions = vec![];
        let first_token = expr.tokens.get(0).unwrap();
        match first_token {
            Token::Minus => {
                // If the first token is an unary negation, evaluate the rest of the tokens and then negate
                let sub_expr = Expression::new(expr.tokens[1..].to_vec());
                expr_instructions.append(&mut Self::render_expression_to_instructions(&sub_expr));
                expr_instructions.push(Instruction::NegateRegister(Register::Rax));
            }
            Token::Int(imm) => {
                // PT: Validate that this is the only token in the stream
                // Move the immediate value into rax
                expr_instructions.push(Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(*imm, Register::Rax)));
            }
            _ => todo!("Unhandled expression contents")
        };
        expr_instructions
        */
        todo!()
    }

    fn render_statement_to_instructions(statement: &Statement) -> Vec<Instruction> {
        let next_free_stack_slot = -(mem::size_of::<u64>() as isize);
        let mut statement_instrs = vec![];
        match statement {
            Statement::Return(ReturnStatement { return_expr }) => {
                /*
                if let Token::Int(imm) = return_expr {
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
                */
                //statement_instrs.append(&mut Self::render_expression_to_instructions(return_expr));
                statement_instrs.append(&mut Self::render_expression_to_instructions(return_expr));
                // Restore the caller's frame pointer
                statement_instrs.push(Instruction::PopIntoReg8(Register::Rbp));
                // Return to caller
                statement_instrs.push(Instruction::Return8);
            }
            Statement::If(if_statement) => {
                // Render the binary expression operands
                let binary_expr = &if_statement.test;
                // Left operand
                /*

                if let Token::Int(val) = binary_expr.left {
                    statement_instrs.push(Instruction::MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset::new(val, next_free_stack_slot, Register::Rbp)));
                    next_free_stack_slot -= mem::size_of::<u64>() as isize;
                }
                else {
                    todo!();
                }

                // Left operand
                if let Token::Int(val) = binary_expr.right {
                    statement_instrs.push(Instruction::MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset::new(val, next_free_stack_slot, Register::Rbp)));
                    next_free_stack_slot -= mem::size_of::<u64>() as isize;
                }
                else {
                    todo!();
                }

                // Comparator
                match binary_expr.operator {
                    BinaryOperator::Equals => todo!(),
                    BinaryOperator::DoesNotEqual => todo!(),
                }
                 */
                todo!()
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
        // Export the function label
        func_instrs.push(Instruction::DirectiveDeclareGlobalSymbol(mangled_function_name.clone()));
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
    use alloc::boxed::Box;
    use alloc::string::String;
    use alloc::vec;
    use alloc::vec::Vec;
    use crate::codegen::{CodeGenerator, Instruction, MoveImm8ToReg8, Register};
    use crate::parser::{InfixOperator, Parser};
    use std::process::Command;
    use std::io::{BufWriter, Write};
    use std::fs::File;
    use std::path::Path;
    use crate::parser::Expr::{IntExpr, OperatorExpr};

    fn run_program(instructions: &Vec<String>) -> i32 {
        //let temp_dir = tempdir()?;
        let temp_dir = Path::new("/Users/philliptennen/Downloads/");
        // clang -e start2 test.s
        let instructions_file_path = temp_dir.join("c_compiler_test_file.s");
        let mut instructions_file = BufWriter::new(File::create(instructions_file_path.clone()).unwrap());
        instructions_file.write(instructions.join("\n").as_bytes()).expect("Failed to write to file");

        let mut assembler = Command::new("/usr/local/opt/llvm/bin/clang");
        let assembler_with_args = assembler
            .current_dir(temp_dir)
            .args(["-e", "start2", instructions_file_path.as_path().to_str().unwrap()]);
        println!("assembler command {assembler_with_args:?}");

        let assembler_status_code = assembler_with_args
            .status()
            .expect("Failed to run assembler");

        println!("Assembler status code: {assembler_status_code}");

        assembler_status_code.code().expect("Process terminated by signal")
    }

    fn generate_assembly_from_source(source: &str) -> Vec<String> {
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new(func);
        let instructions = codegen.generate();
        instructions
    }

    #[test]
    fn test_return_int() {
        // Given a simple function that returns a constant
        let source = r"
        int start2() {
            return 5;
        }";
        // When I compile it
        let instructions = generate_assembly_from_source(source);
        assert_eq!(
            instructions,
            // Then the generated assembly looks correct
            vec![
                ".global _start2",
                "_start2:",
                "push %rbp",
                "mov %rsp, %rbp",
                "mov $5, %rax",
                "pop %rbp",
                "ret",
            ]
        );

        // And the program outputs the correct value
        assert_eq!(run_program(&instructions), 5);
    }

    #[test]
    fn test_return_negative_int() {
        // Given a function that returns a constant with an unary negation applied
        let source = r"
        int start2() {
            return -5;
        }";
        // When I compile it
        let instructions = generate_assembly_from_source(source);
        // Then the generated assembly looks correct
        assert_eq!(
            instructions,
            vec![
                ".global _start2",
                "_start2:",
                "push %rbp",
                "mov %rsp, %rbp",
                "mov $5, %rax",
                "neg %rax",
                "pop %rbp",
                "ret",
            ]
        );
    }

    #[test]
    fn test_expression() {
        assert_eq!(
            CodeGenerator::render_expression_to_instructions(
                &OperatorExpr(
                    Box::new(IntExpr(1)),
                    InfixOperator::Plus,
                    Box::new(IntExpr(2)),
                )
            ),
            vec![
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(1, Register::Rax)),
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(2, Register::Rdx)),
                //Instruction::AddReg8ToReg8(AddReg8ToReg8::new(Register::Rdx, Register::Rax)),
                Instruction::PushFromReg8(Register::Rax),
            ]
        );
    }

    /*
        fn render_expression_to_instructions(expr: &Expression) -> Vec<Instruction> {
        // Given a function that returns a constant with an unary negation applied
        let source = r"
        int start2() {
            return 1 + 2;
        }";
        /*
        // When I compile it
        let instructions = generate_assembly_from_source(source);
        // Then the generated assembly looks correct
        assert_eq!(
            instructions,
            vec![
                ".global _start2",
                "_start2:",
                "push %rbp",
                "mov %rsp, %rbp",
                "mov $1, %rax",
                "push %rax",
                "mov $2, %rax",
                "push %rax",
                "neg %rax",
                "pop %rbp",
                "ret",
            ]
        );
        */

    }
     */
}
