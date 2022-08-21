use crate::lexer::{Lexer, Token};
use alloc::collections::BTreeMap;
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};

use crate::println;

#[derive(Debug, PartialEq)]
pub enum PrimitiveTypeName {
    Int,
    Float,
}

#[derive(Debug, PartialEq)]
pub struct ReturnStatement {
    pub return_value: Token,
}

impl ReturnStatement {
    fn new(return_value: Token) -> Self {
        Self { return_value }
    }
}

#[derive(Debug, PartialEq)]
pub enum Statement {
    Return(ReturnStatement),
}

#[derive(Debug)]
pub struct Function {
    pub return_type: PrimitiveTypeName,
    pub name: String,
    pub statements: Vec<Statement>,
}

impl Function {
    fn new(return_type: PrimitiveTypeName, name: String, statements: Vec<Statement>) -> Self {
        Self {
            return_type,
            name,
            statements,
        }
    }

    /// Ensures that the return statements return types allowed by the function's return type
    fn validate_return_statements(&self) {
        for return_statement in self.statements.iter().filter_map(|stmt| match stmt {
            Statement::Return(return_stmt) => Some(return_stmt),
            _ => None,
        }) {
            println!("Found return statement {return_statement:?}");
        }
    }
}

/*
#[derive(Debug)]
enum AstNode {
    TypeName(PrimitiveTypeName),
    Function(Function),
}
*/

pub struct Parser {
    lexer: Lexer,
}

impl Parser {
    pub fn new(source: &str) -> Self {
        Self {
            lexer: Lexer::new(source),
        }
    }

    fn parse_primitive_type_name(&mut self) -> PrimitiveTypeName {
        let token = self.lexer.next_token().unwrap();
        // Ensure the token is a valid type
        if let Token::Identifier(name) = token {
            return match name.as_str() {
                "int" => PrimitiveTypeName::Int,
                "float" => PrimitiveTypeName::Float,
                _ => panic!("Expected a type name, got '{name}'"),
            };
        }
        panic!("Expected an identifier token, got {token:?}");
    }

    fn parse_identifier(&mut self) -> String {
        self.lexer.match_identifier()
    }

    /*
    fn parse_assignment(&mut self) {
        //
    }
    */
    fn parse_return_statement(&mut self) -> ReturnStatement {
        self.lexer.match_token(Token::Identifier("return".into()));
        let return_value = self
            .lexer
            .next_token()
            .expect("Expected a value to be returned");
        self.lexer.match_token(Token::Semicolon);
        ReturnStatement::new(return_value)
    }

    pub fn parse_function(&mut self) -> Function {
        let return_type = self.parse_primitive_type_name();
        let function_name = self.lexer.match_identifier();
        self.lexer.match_token(Token::ParenLeft);
        self.lexer.match_token(Token::ParenRight);
        self.lexer.match_token(Token::CurlyBraceLeft);

        // Parse a single return statement
        let return_statement = Statement::Return(self.parse_return_statement());
        println!(
            "Found function: fn {function_name}() -> {return_type:?} {{{return_statement:?}}}"
        );

        Function::new(return_type, function_name, vec![return_statement])
    }

    pub fn parse(&mut self) {
        // Expect a function
        self.parse_function();
        todo!()
    }
}

#[cfg(test)]
mod test {
    use crate::lexer::Token;
    use crate::parser::{Parser, PrimitiveTypeName, ReturnStatement, Statement};
    use alloc::vec;

    #[test]
    fn parse_function_returning_int() {
        let source = r"
            int _start() {
                return 5;
            }";
        let mut parser = Parser::new(source);
        let function = parser.parse_function();
        assert_eq!(function.return_type, PrimitiveTypeName::Int);
        assert_eq!(function.name, "_start");
        assert_eq!(
            function.statements,
            vec![Statement::Return(ReturnStatement::new(Token::Int(5)))]
        );
        function.validate_return_statements();
    }

    #[test]
    fn parse_function_returning_float() {
        let source = r"
            float returns_float() {
                return 9.12345;
            }";
        let mut parser = Parser::new(source);
        let function = parser.parse_function();
        assert_eq!(function.return_type, PrimitiveTypeName::Float);
        assert_eq!(function.name, "returns_float");
        assert_eq!(
            function.statements,
            vec![Statement::Return(ReturnStatement::new(Token::Float(
                9.12345
            )))]
        );
        function.validate_return_statements();
    }
}
