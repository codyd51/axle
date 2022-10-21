use crate::parser::{Expr, Function, InfixOperator, ReturnStatement, Statement};
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use core::mem;
use strum::IntoEnumIterator;
use strum_macros::EnumIter;

use crate::println;

#[derive(Debug, PartialEq, EnumIter, Ord, PartialOrd, Eq, Copy, Clone)]
pub enum Register {
    Rax,
    Rbx,
    Rdx,
    Rbp,
    Rsp,
    Rip,
}

impl Register {
    fn asm_name(&self) -> &'static str {
        match self {
            Register::Rax => "rax",
            Register::Rbx => "rax",
            Register::Rdx => "rdx",
            Register::Rbp => "rbp",
            Register::Rsp => "rsp",
            Register::Rip => "rip",
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveReg8ToReg8 {
    pub source: Register,
    pub dest: Register,
}

impl MoveReg8ToReg8 {
    pub fn new(source: Register, dest: Register) -> Self {
        Self { source, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm8ToReg8 {
    pub imm: usize,
    pub dest: Register,
}

impl MoveImm8ToReg8 {
    pub fn new(imm: usize, dest: Register) -> Self {
        Self { imm, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm32ToReg32 {
    pub imm: usize,
    pub dest: Register,
}

impl MoveImm32ToReg32 {
    pub fn new(imm: usize, dest: Register) -> Self {
        Self { imm, dest }
    }
}

#[derive(Debug, PartialEq)]
pub struct MoveImm8ToReg8MemOffset {
    imm: usize,
    offset: isize,
    reg_to_deref: Register,
}

impl MoveImm8ToReg8MemOffset {
    pub fn new(imm: usize, offset: isize, reg_to_deref: Register) -> Self {
        Self {
            imm,
            offset,
            reg_to_deref,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct AddReg32ToReg32 {
    pub augend: Register,
    pub addend: Register,
}

impl AddReg32ToReg32 {
    pub fn new(augend: Register, addend: Register) -> Self {
        Self {
            augend,
            addend,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct SubReg32FromReg32 {
    pub minuend: Register,
    pub subtrahend: Register,
}

impl SubReg32FromReg32 {
    pub fn new(minuend: Register, subtrahend: Register) -> Self {
        Self {
            minuend,
            subtrahend,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct MulReg32ByReg32 {
    pub multiplicand: Register,
    pub multiplier: Register,
}

impl MulReg32ByReg32 {
    pub fn new(multiplicand: Register, multiplier: Register) -> Self {
        Self {
            multiplicand,
            multiplier,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct DivReg32ByReg32 {
    pub dividend: Register,
    pub divisor: Register,
}

impl DivReg32ByReg32 {
    pub fn new(dividend: Register, divisor: Register) -> Self {
        Self {
            dividend,
            divisor,
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum Instruction {
    Return8,
    PushFromReg32(Register),
    PopIntoReg32(Register),
    MoveReg8ToReg8(MoveReg8ToReg8),
    MoveImm8ToReg8(MoveImm8ToReg8),
    MoveImm32ToReg32(MoveImm32ToReg32),
    DirectiveDeclareGlobalSymbol(String),
    DirectiveDeclareLabel(String),
    MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset),
    NegateRegister(Register),
    AddReg8ToReg8(AddReg32ToReg32),
    SubReg32FromReg32(SubReg32FromReg32),
    MulReg32ByReg32(MulReg32ByReg32),
    DivReg32ByReg32(DivReg32ByReg32),
}

impl Instruction {
    fn render(&self) -> String {
        match self {
            Instruction::Return8 => "ret".into(),
            Instruction::PushFromReg32(src) => format!("push %{}", src.asm_name()),
            Instruction::PopIntoReg32(dst) => format!("pop %{}", dst.asm_name()),
            Instruction::MoveReg8ToReg8(MoveReg8ToReg8 { source, dest }) => {
                format!("mov %{}, %{}", source.asm_name(), dest.asm_name())
            }
            Instruction::MoveImm8ToReg8(MoveImm8ToReg8 { imm, dest }) => {
                format!("mov ${imm}, %{}", dest.asm_name())
            }
            Instruction::DirectiveDeclareGlobalSymbol(symbol_name) => {
                format!(".global {symbol_name}")
            }
            Instruction::DirectiveDeclareLabel(label_name) => format!("{label_name}:"),
            Instruction::NegateRegister(reg) => format!("neg %{}", reg.asm_name()),
            Instruction::AddReg8ToReg8(AddReg32ToReg32 { augend, addend }) => {
                format!("add %{}, %{}", augend.asm_name(), addend.asm_name())
            }
            _ => todo!(),
        }
    }
}

#[derive(Debug)]
pub struct CodeGenerator {
    //function: Function,
}

impl CodeGenerator {
    pub fn new() -> Self {
        //Self { function }
        Self {}
    }

    fn render_expression_to_instructions(&mut self, expr: &Expr) -> Vec<Instruction> {
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

    fn codegen_statement(&self, statement: &Statement) -> Vec<Instruction> {
        let mut statement_instrs = vec![];
        match statement {
            Statement::Return(ReturnStatement { return_expr }) => {
                // The expression's return value will be in rax
                statement_instrs.append(&mut self.codegen_expression(return_expr));
                // Restore the caller's frame pointer
                statement_instrs.push(Instruction::PopIntoReg32(Register::Rbp));
                // Return to caller
                statement_instrs.push(Instruction::Return8);
            }
            _ => todo!(),
        }
        statement_instrs
    }

    // TODO(PT): To drop
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
                    statement_instrs.push(Instruction::PopIntoReg32(Register::Rbp));
                    // Return to caller
                    statement_instrs.push(Instruction::Return8);
                }
                else {
                    todo!("Unhandled return type");
                }
                */
                //statement_instrs.append(&mut Self::render_expression_to_instructions(return_expr));
                //statement_instrs.append(&mut Self::render_expression_to_instructions(return_expr));
                // Restore the caller's frame pointer
                statement_instrs.push(Instruction::PopIntoReg32(Register::Rbp));
                // Return to caller
                statement_instrs.push(Instruction::Return8);
            }
            Statement::If(if_statement) => {
                // Render the binary expression operands
                match &if_statement.test {
                    Expr::TestExpr(lhs, rhs) => {
                        //
                    }
                    _ => panic!("Unrecognized expression within an if"),
                };
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

    pub fn codegen_expression(&self, expr: &Expr) -> Vec<Instruction> {
        match expr {
            Expr::OperatorExpr(lhs, op, rhs) => {
                let mut expr_instrs = vec![];
                /*
                println!(
                    "Compiling infix expression:\
                \tLHS: {lhs:?}\
                \tOp:  {op:?}\
                \tRHS: {rhs:?}"
                );
                */

                let mut lhs_instrs = self.codegen_expression(lhs);
                expr_instrs.append(&mut lhs_instrs);
                // LHS computed value is in rax. Push to stack
                expr_instrs.push(Instruction::PushFromReg32(Register::Rax));

                let mut rhs_instrs = self.codegen_expression(rhs);
                expr_instrs.append(&mut rhs_instrs);
                // RHS computed value is in rax. Push to stack
                expr_instrs.push(Instruction::PushFromReg32(Register::Rax));

                // Apply the operator
                match op {
                    InfixOperator::Plus => {
                        // Pop LHS and RHS into working registers
                        // RHS into rax
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rax));
                        // LHS into rbx
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rbx));

                        expr_instrs.push(Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx)));
                    }
                    InfixOperator::Minus => {
                        // Pop LHS and RHS into working registers
                        // We want the result to end up in rax, so pop the minuend into Rax for convenience
                        // (subl stores the result in the dst register)
                        // RHS into rbx
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rbx));
                        // LHS into rax
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rax));
                        expr_instrs.push(Instruction::SubReg32FromReg32(SubReg32FromReg32::new(Register::Rax, Register::Rbx)));
                    }
                    InfixOperator::Asterisk => {
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rax));
                        expr_instrs.push(Instruction::PopIntoReg32(Register::Rbx));
                        expr_instrs.push(Instruction::MulReg32ByReg32(MulReg32ByReg32::new(Register::Rax, Register::Rbx)));
                    }
                    _ => todo!(),
                }
                expr_instrs
            }
            Expr::IntExpr(val) => {
                vec![Instruction::MoveImm32ToReg32(MoveImm32ToReg32::new(
                    *val,
                    Register::Rax,
                ))]
            }
            _ => todo!(),
        }
    }

    pub(crate) fn codegen_function(&self, function: &Function) -> Vec<Instruction> {
        let mut func_instrs = vec![];
        // Mangle the function name with a leading underscore
        let mangled_function_name = format!("_{}", function.name);
        // Export the function label
        func_instrs.push(Instruction::DirectiveDeclareGlobalSymbol(
            mangled_function_name.clone(),
        ));
        // Declare a label denoting the start of the function
        func_instrs.push(Instruction::DirectiveDeclareLabel(mangled_function_name));
        // Save the caller's frame pointer
        func_instrs.push(Instruction::PushFromReg32(Register::Rbp));
        // Set up a new stack frame by storing the starting/base stack pointer in the base pointer
        func_instrs.push(Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(
            Register::Rsp,
            Register::Rbp,
        )));

        // Visit each statement in the function
        for statement in function.body.statements.iter() {
            println!("Visting statement {statement:?}");
            let mut statement_instrs = self.codegen_statement(&statement);
            func_instrs.append(&mut statement_instrs);
        }

        func_instrs
    }

    // TODO(PT): Drop
    fn render_function_to_instructions(function: &Function) -> Vec<Instruction> {
        let mut func_instrs = vec![];
        // Mangle the function name with a leading underscore
        let mangled_function_name = format!("_{}", function.name);
        // Export the function label
        func_instrs.push(Instruction::DirectiveDeclareGlobalSymbol(
            mangled_function_name.clone(),
        ));
        // Declare a label denoting the start of the function
        func_instrs.push(Instruction::DirectiveDeclareLabel(mangled_function_name));
        // Save the caller's frame pointer
        func_instrs.push(Instruction::PushFromReg32(Register::Rbp));
        // Set up a new stack frame by storing the starting/base stack pointer in the base pointer
        func_instrs.push(Instruction::MoveReg8ToReg8(MoveReg8ToReg8::new(
            Register::Rsp,
            Register::Rbp,
        )));

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
        todo!()
    }

    /*
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
    */
}

#[cfg(test)]
mod test {
    use crate::codegen::{AddReg32ToReg32, CodeGenerator, Instruction, MoveImm8ToReg8, Register};
    use crate::parser::Expr::{IntExpr, OperatorExpr};
    use crate::parser::{InfixOperator, Parser};
    use alloc::boxed::Box;
    use alloc::string::String;
    use alloc::vec;
    use alloc::vec::Vec;
    use std::fs::File;
    use std::io::{BufWriter, Write};
    use std::path::Path;
    use std::process::Command;

    fn run_program(instructions: &Vec<String>) -> i32 {
        //let temp_dir = tempdir()?;
        let temp_dir = Path::new("/Users/philliptennen/Downloads/");
        // clang -e start2 test.s
        let instructions_file_path = temp_dir.join("c_compiler_test_file.s");
        let mut instructions_file =
            BufWriter::new(File::create(instructions_file_path.clone()).unwrap());
        instructions_file
            .write(instructions.join("\n").as_bytes())
            .expect("Failed to write to file");

        let mut assembler = Command::new("/usr/local/opt/llvm/bin/clang");
        let assembler_with_args = assembler.current_dir(temp_dir).args([
            "-e",
            "start2",
            instructions_file_path.as_path().to_str().unwrap(),
        ]);
        println!("assembler command {assembler_with_args:?}");

        let assembler_status_code = assembler_with_args
            .status()
            .expect("Failed to run assembler");

        println!("Assembler status code: {assembler_status_code}");

        assembler_status_code
            .code()
            .expect("Process terminated by signal")
    }

    fn generate_assembly_from_source(source: &str) -> Vec<String> {
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
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
        //let source = "void foo() { int a = 1 + 2 + 3; }";
        let source = "void foo() {}";
        let mut parser = Parser::new(source);
        let func = parser.parse_function();
        let codegen = CodeGenerator::new();
        assert_eq!(
            codegen.codegen_expression(&OperatorExpr(
                Box::new(IntExpr(1)),
                InfixOperator::Plus,
                Box::new(IntExpr(2)),
            )),
            vec![
                // Push LHS onto the stack
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(1, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                // Push RHS onto the stack
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(2, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                // Pop RHS into rax
                Instruction::PopIntoReg32(Register::Rax),
                // Pop LHS into rbx
                Instruction::PopIntoReg32(Register::Rbx),
                // Add and store in rax
                Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx)),
            ]
        );
        //let instrs = codegen.generate();

        assert_eq!(
            codegen.codegen_expression(&OperatorExpr(
                Box::new(OperatorExpr(
                    Box::new(IntExpr(3)),
                    InfixOperator::Plus,
                    Box::new(IntExpr(7)),
                )),
                InfixOperator::Plus,
                Box::new(IntExpr(2)),
            )),
            vec![
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(3, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(7, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::MoveImm8ToReg8(MoveImm8ToReg8::new(2, Register::Rax)),
                Instruction::PushFromReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rax),
                Instruction::PopIntoReg32(Register::Rbx),
                Instruction::AddReg8ToReg8(AddReg32ToReg32::new(Register::Rax, Register::Rbx))
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
