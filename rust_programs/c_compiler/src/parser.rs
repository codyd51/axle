use crate::lexer::{Lexer, Token};
use alloc::collections::BTreeMap;
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use core::fmt::Binary;

use crate::println;

#[derive(Debug, PartialEq)]
pub enum PrimitiveTypeName {
    Int,
    Float,
    Void,
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
pub struct IfStatement {
    test: BinaryExpression,
    consequent: BlockStatement,
}

impl IfStatement {
    fn new(test: BinaryExpression, consequent: BlockStatement) -> Self {
        Self {
            test,
            consequent,
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct BlockStatement {
    pub statements: Vec<Statement>,
}

impl BlockStatement {
    fn new(statements: Vec<Statement>) -> Self {
        Self { statements }
    }
}

#[derive(Debug, PartialEq)]
pub enum Statement {
    Return(ReturnStatement),
    If(IfStatement),
    Block(BlockStatement),
}

#[derive(Debug, PartialEq)]
pub enum BinaryOperator {
    Equals,
    DoesNotEqual,
}

impl From<&str> for BinaryOperator {
    fn from(name: &str) -> Self {
        match name {
            "==" => Self::Equals,
            "!=" => Self::DoesNotEqual,
            _ => panic!("Invalid binary operand {name}"),
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct BinaryExpression {
    operator: BinaryOperator,
    left: Token,
    right: Token,
}

impl BinaryExpression {
    fn new(operator: BinaryOperator, left: Token, right: Token) -> Self {
        Self { operator, left, right }
    }
}

#[derive(Debug)]
pub struct Function {
    pub return_type: PrimitiveTypeName,
    pub name: String,
    pub body: BlockStatement,
}

impl Function {
    fn new(return_type: PrimitiveTypeName, name: String, body: BlockStatement) -> Self {
        let func = Self {
            return_type,
            name,
            body,
        };
        func.validate_return_statements();

        func
    }

    /// Ensures that the return statements return types allowed by the function's return type
    fn validate_return_statements(&self) {
        let statements = &self.body.statements;
        for return_statement in statements.iter().filter_map(|stmt| match stmt {
            Statement::Return(return_stmt) => Some(return_stmt),
            _ => None,
        }) {
            println!("Found return statement {return_statement:?}");
        }
    }
}

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
                "void" => PrimitiveTypeName::Void,
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

    fn parse_binary_operator(&mut self) -> BinaryOperator {
        let operator_tok = self.lexer.match_identifier();
        BinaryOperator::from(operator_tok.as_str())
    }

    fn parse_binary_expr(&mut self) -> BinaryExpression {
        // Parse the left side of the expression
        let left = self.lexer.next_token().unwrap();
        let operator = self.parse_binary_operator();
        let right = self.lexer.next_token().unwrap();
        BinaryExpression::new(operator, left, right)
    }

    fn parse_block(&mut self) -> BlockStatement {
        let mut statements = vec![];
        self.lexer.match_token(Token::CurlyBraceLeft);

        loop {
            // Check whether the next statement begins with a keyword
            let next_token = self.lexer.peek_token().expect("Expected a token within the function body");
            if let Token::Identifier(name) = next_token {
                let mut statements_in_expr = match name.as_str() {
                    "return" => Statement::Return(self.parse_return_statement()),
                    "if" => Statement::If(self.parse_if()),
                    _ => panic!("Unrecognized token {name}"),
                };
                statements.push(statements_in_expr);
            } else if let Token::CurlyBraceRight = next_token {
                // End of function body
                self.lexer.match_token(Token::CurlyBraceRight);
                break;
            } else {
                panic!("Unrecognized token {next_token:?}");
            }
        }

        BlockStatement::new(statements)
    }

    fn parse_if(&mut self) -> IfStatement {
        self.lexer.match_token(Token::Identifier("if".into()));

        // Parse the test
        self.lexer.match_token(Token::ParenLeft);
        let test = self.parse_binary_expr();
        self.lexer.match_token(Token::ParenRight);

        // Parse the body
        let consequent = self.parse_block();

        IfStatement::new(test, consequent)
    }

    pub fn parse_function(&mut self) -> Function {
        let return_type = self.parse_primitive_type_name();
        let function_name = self.lexer.match_identifier();
        self.lexer.match_token(Token::ParenLeft);
        self.lexer.match_token(Token::ParenRight);
        let body = self.parse_block();

        println!(
            "Found function: fn {function_name}() -> {return_type:?} {{{body:?}}}"
        );

        Function::new(return_type, function_name, body)
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
    use crate::parser::{BinaryExpression, BinaryOperator, BlockStatement, IfStatement, Parser, PrimitiveTypeName, ReturnStatement, Statement};
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
            function.body.statements,
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
            function.body.statements,
            vec![Statement::Return(ReturnStatement::new(Token::Float(
                9.12345
            )))]
        );
        function.validate_return_statements();
    }

    #[test]
    fn parse_if() {
        let source = r"
        void f() {
            if (1 == 2) {
                return 3;
            }
            return 4;
        }";
        let mut parser = Parser::new(source);
        let function = parser.parse_function();
        assert_eq!(function.return_type, PrimitiveTypeName::Void);
        assert_eq!(function.name, "f");
        assert_eq!(
            function.body.statements,
            vec![
                Statement::If(
                    IfStatement::new(
                        BinaryExpression::new(BinaryOperator::Equals, Token::Int(1), Token::Int(2)),
                        BlockStatement::new(
                            vec![
                                Statement::Return(ReturnStatement::new(Token::Int(3))),
                            ]
                        )
                    )
                ),
                Statement::Return(ReturnStatement::new(Token::Int(4))),
            ]
        );
        function.validate_return_statements();
    }
}
