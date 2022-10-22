use crate::parser::{Expr, Function, InfixOperator, ReturnStatement, Statement};
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use alloc::string::ToString;
use core::mem;
use crate::instructions::{AddRegToReg, Instr, MoveImmToReg, MoveRegToReg, MulRegByReg, SubRegFromReg};

use crate::println;
use crate::prelude::*;

#[derive(Debug)]
pub struct CodeGenerator {
    //function: Function,
}

impl CodeGenerator {
    pub fn new() -> Self {
        //Self { function }
        Self {}
    }

    fn render_expression_to_instructions(&mut self, expr: &Expr) -> Vec<Instr> {
        /*
        let mut expr_instructions = vec![];
        let first_token = expr.tokens.get(0).unwrap();
        match first_token {
            Token::Minus => {
                // If the first token is an unary negation, evaluate the rest of the tokens and then negate
                let sub_expr = Expression::new(expr.tokens[1..].to_vec());
                expr_instructions.append(&mut Self::render_expression_to_instructions(&sub_expr));
                expr_instructions.push(Instruction::NegateRegister(Rax));
            }
            Token::Int(imm) => {
                // PT: Validate that this is the only token in the stream
                // Move the immediate value into rax
                expr_instructions.push(Instruction::MoveImmToReg(MoveImmToReg::new(*imm, Rax)));
            }
            _ => todo!("Unhandled expression contents")
        };
        expr_instructions
        */
        todo!()
    }

    fn codegen_statement(&self, statement: &Statement) -> Vec<Instr> {
        let mut statement_instrs = vec![];
        match statement {
            Statement::Return(ReturnStatement { return_expr }) => {
                // The expression's return value will be in rax
                statement_instrs.append(&mut self.codegen_expression(return_expr));
                // Restore the caller's frame pointer
                statement_instrs.push(Instr::PopIntoReg(RegisterView::rbp()));
                // Return to caller
                statement_instrs.push(Instr::Return);
            }
            _ => todo!(),
        }
        statement_instrs
    }

    // TODO(PT): To drop
    fn render_statement_to_instructions(statement: &Statement) -> Vec<Instr> {
        let next_free_stack_slot = -(mem::size_of::<u64>() as isize);
        let mut statement_instrs = vec![];
        match statement {
            Statement::Return(ReturnStatement { return_expr }) => {
                /*
                if let Token::Int(imm) = return_expr {
                    // Move the immediate return value into rax
                    statement_instrs.push(Instruction::MoveImmToReg(MoveImmToReg::new(*imm, Rax)));
                    // Restore the caller's frame pointer
                    statement_instrs.push(Instruction::PopIntoReg32(Rbp));
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
                statement_instrs.push(Instr::PopIntoReg(RegisterView::rbp()));
                // Return to caller
                statement_instrs.push(Instr::Return);
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
                    statement_instrs.push(Instruction::MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset::new(val, next_free_stack_slot, Rbp)));
                    next_free_stack_slot -= mem::size_of::<u64>() as isize;
                }
                else {
                    todo!();
                }

                // Left operand
                if let Token::Int(val) = binary_expr.right {
                    statement_instrs.push(Instruction::MoveImm8ToReg8MemOffset(MoveImm8ToReg8MemOffset::new(val, next_free_stack_slot, Rbp)));
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

    pub fn codegen_expression(&self, expr: &Expr) -> Vec<Instr> {
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
                expr_instrs.push(Instr::PushFromReg(RegisterView::rax()));

                let mut rhs_instrs = self.codegen_expression(rhs);
                expr_instrs.append(&mut rhs_instrs);
                // RHS computed value is in rax. Push to stack
                expr_instrs.push(Instr::PushFromReg(RegisterView::rax()));

                // Apply the operator
                match op {
                    InfixOperator::Plus => {
                        // Pop LHS and RHS into working registers
                        // RHS into rax
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rax()));
                        // LHS into rbx
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rbx()));

                        expr_instrs.push(Instr::AddRegToReg(AddRegToReg::new(RegisterView::rax(), RegisterView::rbx())));
                    }
                    InfixOperator::Minus => {
                        // Pop LHS and RHS into working registers
                        // We want the result to end up in rax, so pop the minuend into Rax for convenience
                        // (subl stores the result in the dst register)
                        // RHS into rbx
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rbx()));
                        // LHS into rax
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rax()));
                        expr_instrs.push(Instr::SubRegFromReg(SubRegFromReg::new(RegisterView::rax(), RegisterView::rbx())));
                    }
                    InfixOperator::Asterisk => {
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rax()));
                        expr_instrs.push(Instr::PopIntoReg(RegisterView::rbx()));
                        expr_instrs.push(Instr::MulRegByReg(MulRegByReg::new(RegisterView::rax(), RegisterView::rbx())));
                    }
                    _ => todo!(),
                }
                expr_instrs
            }
            Expr::IntExpr(val) => {
                vec![Instr::MoveImmToReg(MoveImmToReg::new(
                    *val,
                    RegisterView::rax(),
                ))]
            }
            _ => todo!(),
        }
    }

    pub(crate) fn codegen_function(&self, function: &Function) -> Vec<Instr> {
        let mut func_instrs = vec![];
        // Mangle the function name with a leading underscore
        let mangled_function_name = format!("_{}", function.name);
        // Export the function label
        func_instrs.push(Instr::DirectiveDeclareGlobalSymbol(
            mangled_function_name.clone(),
        ));
        // Declare a label denoting the start of the function
        func_instrs.push(Instr::DirectiveDeclareLabel(mangled_function_name));
        // Save the caller's frame pointer
        func_instrs.push(Instr::PushFromReg(RegisterView::rbp()));
        // Set up a new stack frame by storing the starting/base stack pointer in the base pointer
        func_instrs.push(Instr::MoveRegToReg(MoveRegToReg::new(
            RegisterView::rsp(),
            RegisterView::rbp(),
        )));

        // Visit each statement in the function
        for statement in function.body.statements.iter() {
            //println!("Visiting statement {statement:?}");
            let mut statement_instrs = self.codegen_statement(&statement);
            func_instrs.append(&mut statement_instrs);
        }

        func_instrs
    }

    // TODO(PT): Drop
    fn render_function_to_instructions(function: &Function) -> Vec<Instr> {
        let mut func_instrs = vec![];
        // Mangle the function name with a leading underscore
        let mangled_function_name = format!("_{}", function.name);
        // Export the function label
        func_instrs.push(Instr::DirectiveDeclareGlobalSymbol(
            mangled_function_name.clone(),
        ));
        // Declare a label denoting the start of the function
        func_instrs.push(Instr::DirectiveDeclareLabel(mangled_function_name));
        // Save the caller's frame pointer
        func_instrs.push(Instr::PushFromReg(RegisterView::rbp()));
        // Set up a new stack frame by storing the starting/base stack pointer in the base pointer
        func_instrs.push(Instr::MoveRegToReg(MoveRegToReg::new(
            RegisterView::rsp(),
            RegisterView::rbp(),
        )));

        // Visit each statement in the function
        for statement in function.body.statements.iter() {
            println!("Visting statement {statement:?}");
            let mut statement_instrs = Self::render_statement_to_instructions(&statement);
            func_instrs.append(&mut statement_instrs);
        }

        func_instrs
    }

    fn render_instructions_to_assembly(instructions: Vec<Instr>) -> Vec<String> {
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
    use crate::codegen::CodeGenerator;
    use crate::instructions::{AddRegToReg, Instr, MoveImmToReg};
    use crate::parser::Expr::{IntExpr, OperatorExpr};
    use crate::parser::{InfixOperator, Parser};
    use crate::prelude::*;
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
                Instr::MoveImmToReg(MoveImmToReg::new(1, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                // Push RHS onto the stack
                Instr::MoveImmToReg(MoveImmToReg::new(2, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                // Pop RHS into rax
                Instr::PopIntoReg(RegisterView::rax()),
                // Pop LHS into rbx
                Instr::PopIntoReg(RegisterView::rbx()),
                // Add and store in rax
                Instr::AddRegToReg(AddRegToReg::new(RegisterView::rax(), RegisterView::rbx())),
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
                Instr::MoveImmToReg(MoveImmToReg::new(3, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(7, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddRegToReg(AddRegToReg::new(RegisterView::rax(), RegisterView::rbx())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::MoveImmToReg(MoveImmToReg::new(2, RegisterView::rax())),
                Instr::PushFromReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rax()),
                Instr::PopIntoReg(RegisterView::rbx()),
                Instr::AddRegToReg(AddRegToReg::new(RegisterView::rax(), RegisterView::rbx()))
            ]
        );
    }

    #[test]
    fn test_register_view() {
        assert_eq!(RegisterView::al().asm_name(), "al");
        assert_eq!(RegisterView::ah().asm_name(), "ah");
        assert_eq!(RegisterView::ax().asm_name(), "ax");
        assert_eq!(RegisterView::eax().asm_name(), "eax");
        assert_eq!(RegisterView::rax().asm_name(), "rax");
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