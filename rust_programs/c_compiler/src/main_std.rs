use crate::parser::Parser;
use crate::codegen::CodeGenerator;
use std::error;

pub fn main() -> Result<(), Box<dyn error::Error>> {
    println!("Running with std");

    let source = r"
        int _start() {
            return 5;
        }";
    let mut parser = Parser::new(source);
    let func = parser.parse_function();
    let codegen = CodeGenerator::new(func);
    codegen.generate();

    Ok(())
}
