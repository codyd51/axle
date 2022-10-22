use crate::lexer::{Lexer, Token};
use crate::parser::Expr::{
    AssignmentExpr, CallExpr, FloatExpr, IntExpr, NameExpr, OperatorExpr, PrefixExpr, TernaryExpr,
    TestExpr,
};
use alloc::boxed::Box;
use alloc::string::ToString;
use alloc::{format, vec};
use alloc::{string::String, vec::Vec};
use axle_rt::AmcMessage;
use core::fmt::{Display, Formatter};

use crate::println;

#[derive(Debug, PartialEq)]
pub enum PrimitiveTypeName {
    Int,
    Float,
    Void,
}

impl TryFrom<Token> for PrimitiveTypeName {
    type Error = ();

    fn try_from(tok: Token) -> Result<Self, Self::Error> {
        match tok {
            Token::Identifier(ident) => match ident.as_str() {
                "int" => Ok(PrimitiveTypeName::Int),
                "float" => Ok(PrimitiveTypeName::Float),
                "void" => Ok(PrimitiveTypeName::Void),
                _ => Err(()),
            },
            _ => Err(()),
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct ReturnStatement {
    pub return_expr: Expr,
}

impl ReturnStatement {
    fn new(return_expr: Expr) -> Self {
        Self { return_expr }
    }
}

#[derive(Debug, PartialEq)]
pub struct IfStatement {
    pub test: Expr,
    pub consequent: BlockStatement,
}

impl IfStatement {
    fn new(test: Expr, consequent: BlockStatement) -> Self {
        Self { test, consequent }
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
pub struct DeclareStatement {
    name: String,
    value: Expr,
}

impl DeclareStatement {
    fn new(name: &str, value: Expr) -> Self {
        Self {
            name: name.to_string(),
            value,
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum Statement {
    //Declare(DeclareStatement),
    Return(ReturnStatement),
    If(IfStatement),
    Block(BlockStatement),
}

#[derive(Debug, PartialOrd, Ord, PartialEq, Eq, Copy, Clone)]
enum Precedence {
    Lowest,
    Assignment,
    Ternary,
    Sum,
    Product,
    Exponent,
    Prefix,
    Postfix,
    Call,
}

impl Precedence {
    fn previous(&self) -> Precedence {
        match self {
            Precedence::Lowest => Precedence::Lowest,
            Precedence::Assignment => Precedence::Lowest,
            Precedence::Ternary => Precedence::Assignment,
            Precedence::Sum => Precedence::Ternary,
            Precedence::Product => Precedence::Sum,
            Precedence::Exponent => Precedence::Product,
            Precedence::Prefix => Precedence::Exponent,
            Precedence::Postfix => Precedence::Prefix,
            Precedence::Call => Precedence::Postfix,
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum PrefixOperator {
    Plus,
    Minus,
    Tilde,
    Bang,
    ParenLeft,
}

impl PrefixOperator {
    fn is_prefix_operator(token: &Token) -> bool {
        let maybe_as_op: Result<PrefixOperator, ()> = (token.clone()).try_into();
        maybe_as_op.is_ok()
    }
}

impl TryFrom<Token> for PrefixOperator {
    type Error = ();

    fn try_from(tok: Token) -> Result<Self, Self::Error> {
        match tok {
            Token::Plus => Ok(PrefixOperator::Plus),
            Token::Minus => Ok(PrefixOperator::Minus),
            Token::Tilde => Ok(PrefixOperator::Tilde),
            Token::Bang => Ok(PrefixOperator::Bang),
            Token::ParenLeft => Ok(PrefixOperator::ParenLeft),
            _ => Err(()),
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum InfixOperator {
    Plus,
    Minus,
    Asterisk,
    ParenLeft,
    Carat,
    Equals,
    ForwardSlash,
    Question,
}

impl InfixOperator {
    fn is_token_infix(token: &Token) -> bool {
        let maybe_as_op: Result<InfixOperator, ()> = (token.clone()).try_into();
        maybe_as_op.is_ok()
    }
}

impl TryFrom<Token> for InfixOperator {
    type Error = ();

    fn try_from(tok: Token) -> Result<Self, Self::Error> {
        match tok {
            Token::Plus => Ok(InfixOperator::Plus),
            Token::Minus => Ok(InfixOperator::Minus),
            Token::Asterisk => Ok(InfixOperator::Asterisk),
            Token::ParenLeft => Ok(InfixOperator::ParenLeft),
            Token::Carat => Ok(InfixOperator::Carat),
            Token::Equals => Ok(InfixOperator::Equals),
            Token::ForwardSlash => Ok(InfixOperator::ForwardSlash),
            Token::Question => Ok(InfixOperator::Question),
            _ => Err(()),
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum Expr {
    NameExpr(Token),
    IntExpr(usize),
    FloatExpr(f64),
    PrefixExpr(PrefixOperator, Box<Expr>),
    OperatorExpr(Box<Expr>, InfixOperator, Box<Expr>),
    CallExpr(Box<Expr>, Vec<Expr>),
    AssignmentExpr(String, Box<Expr>),
    TernaryExpr(Box<Expr>, Box<Expr>, Box<Expr>),
    TestExpr(Box<Expr>, Box<Expr>),
}

impl Display for Expr {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        match self {
            NameExpr(t) => {
                let name = match t {
                    Token::Identifier(name) => name,
                    _ => panic!("Unexpected token type"),
                };
                write!(f, "{name}")
            }
            IntExpr(val) => {
                write!(f, "{val}")
            }
            FloatExpr(val) => {
                write!(f, "{val}")
            }
            PrefixExpr(op, expr) => {
                let op_str = match op {
                    PrefixOperator::Plus => "+",
                    PrefixOperator::Minus => "-",
                    PrefixOperator::Tilde => "~",
                    PrefixOperator::Bang => "!",
                    _ => panic!("Unexpected token type"),
                };
                write!(f, "({op_str}{expr})")
            }
            OperatorExpr(lhs, op_tok, rhs) => {
                let op_str = match op_tok {
                    InfixOperator::Plus => "+",
                    InfixOperator::Minus => "-",
                    InfixOperator::Asterisk => "*",
                    InfixOperator::Carat => "^",
                    InfixOperator::ForwardSlash => "/",
                    InfixOperator::Equals => panic!("Should be handled by AssignmentExpr"),
                    InfixOperator::ParenLeft => panic!("Should be handled by CallExpr"),
                    InfixOperator::Question => panic!("Should be handled by TernaryExpr"),
                };
                write!(f, "({lhs} {op_str} {rhs})")
            }
            CallExpr(left, args) => {
                let formatted_args: Vec<String> = args.iter().map(|a| format!("{a}")).collect();
                write!(f, "{left}({})", formatted_args.join(", "))
            }
            AssignmentExpr(name, rhs) => {
                write!(f, "({name} = {rhs})")
            }
            TernaryExpr(cond, then_expr, else_expr) => {
                write!(f, "({cond} ? {then_expr} : {else_expr})")
            }
            TestExpr(lhs, rhs) => {
                write!(f, "({lhs} == {rhs}")
            }
        }
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
            //println!("Found return statement {return_statement:?}");
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
        let maybe_as_primitive_type = token.try_into();
        maybe_as_primitive_type.unwrap()
    }

    fn parse_identifier(&mut self) -> String {
        self.lexer.match_identifier()
    }

    fn get_precedence_of_next_token(&mut self) -> Precedence {
        let peek = self.lexer.peek_token();
        if peek.is_none() {
            return Precedence::Lowest;
        }
        let peek = peek.unwrap();

        // Not an infix operator, lowest precedence
        if !InfixOperator::is_token_infix(&peek) {
            return Precedence::Lowest;
        }

        let peek = peek.try_into().unwrap();
        match peek {
            InfixOperator::Plus | InfixOperator::Minus => Precedence::Sum,
            InfixOperator::Asterisk => Precedence::Product,
            InfixOperator::ForwardSlash => Precedence::Product,
            InfixOperator::ParenLeft => Precedence::Call,
            InfixOperator::Carat => Precedence::Exponent,
            InfixOperator::Equals => Precedence::Assignment,
            InfixOperator::Question => Precedence::Ternary,
        }
    }

    fn parse_expression(&mut self) -> Expr {
        self.parse_expression_with_precedence(None)
    }

    fn parse_expression_with_precedence(&mut self, precedence: Option<Precedence>) -> Expr {
        let next_token = self.lexer.next_token().unwrap();
        // Prefix parsers
        let mut lhs = {
            if let Token::Identifier(_) = next_token {
                NameExpr(next_token)
            } else if let Token::Int(val) = next_token {
                IntExpr(val)
            } else if let Token::Float(val) = next_token {
                FloatExpr(val)
            } else if let Token::ParenLeft = next_token {
                let inner = self.parse_expression();
                self.lexer.match_token(Token::ParenRight);
                inner
            } else if let Ok(prefix_operator) = next_token.try_into() {
                PrefixExpr(
                    prefix_operator,
                    Box::new(self.parse_expression_with_precedence(Some(Precedence::Prefix))),
                )
            } else {
                panic!("Unrecognized start to expression");
            }
        };

        // Handle infix parsers
        while precedence.unwrap_or(Precedence::Lowest) < self.get_precedence_of_next_token() {
            let peek = self.lexer.peek_token();
            if peek.is_none() {
                // End of token stream, no possibility of more operators
                return lhs;
            }
            let peek = peek.unwrap();

            if !InfixOperator::is_token_infix(&peek) {
                panic!("Expected a RHS");
            }

            lhs = {
                let precedence = self.get_precedence_of_next_token();

                if peek == Token::ParenLeft {
                    // Function call
                    self.lexer.match_token(Token::ParenLeft);
                    let mut args = vec![];
                    if self.lexer.peek_token() != Some(Token::ParenRight) {
                        // Parse comma-separated arguments until we hit ')'
                        loop {
                            args.push(self.parse_expression());
                            if self.lexer.peek_token() == Some(Token::ParenRight) {
                                break;
                            }
                            self.lexer.match_token(Token::Comma);
                        }
                    }
                    self.lexer.match_token(Token::ParenRight);
                    CallExpr(Box::new(lhs), args)
                } else if peek == Token::Equals {
                    self.lexer.match_token(Token::Equals);

                    // Is this an assignment ('=') or a test ('==')?
                    if self.lexer.peek_token().unwrap() == Token::Equals {
                        // Test
                        self.lexer.match_token(Token::Equals);
                        TestExpr(Box::new(lhs), Box::new(self.parse_expression()))
                    } else {
                        // Assignment
                        // LHS must be an identifier
                        let label = match lhs {
                            NameExpr(tok) => match tok {
                                Token::Identifier(name) => name,
                                _ => panic!("Expected an identifier token"),
                            },
                            _ => panic!("Expected a name as the LHS of an assignment"),
                        };
                        AssignmentExpr(
                            label,
                            Box::new(
                                self.parse_expression_with_precedence(Some(precedence.previous())),
                            ),
                        )
                    }
                } else if peek == Token::Question {
                    self.lexer.match_token(Token::Question);
                    let then_expr = self.parse_expression();
                    self.lexer.match_token(Token::Colon);
                    let else_expr =
                        self.parse_expression_with_precedence(Some(Precedence::Ternary.previous()));
                    TernaryExpr(Box::new(lhs), Box::new(then_expr), Box::new(else_expr))
                } else {
                    // Consume the infix operator
                    let operator = self.lexer.next_token().unwrap();

                    // The '^' operator is right-associative
                    let rhs_precedence = match operator {
                        Token::Carat => precedence.previous(),
                        _ => precedence,
                    };

                    let rhs = self.parse_expression_with_precedence(Some(rhs_precedence));
                    OperatorExpr(Box::new(lhs), operator.try_into().unwrap(), Box::new(rhs))
                }
            };
        }
        lhs
    }

    fn parse_return_statement(&mut self) -> ReturnStatement {
        self.lexer.match_token(Token::Identifier("return".into()));

        let return_expr = self.parse_expression();
        self.lexer.match_token(Token::Semicolon);
        ReturnStatement::new(return_expr)
    }

    fn parse_block(&mut self) -> BlockStatement {
        let mut statements = vec![];
        self.lexer.match_token(Token::CurlyBraceLeft);

        loop {
            // Start of a statement
            let next_token = self
                .lexer
                .peek_token()
                .expect("Expected a token within the function body");

            // Is it a type declaration?
            if let Ok(primitive_type) = PrimitiveTypeName::try_from(next_token.clone()) {
                //PrimitiveTypeName::try_from(self.lexer.next_token().unwrap())
                // Consume the type name
                self.lexer.next_token();

                println!("Parsing primitive type {primitive_type:?}");
                let variable_name = self.lexer.match_identifier();
                println!("variable name {variable_name:?}");
                self.lexer.match_token(Token::Equals);
                let value = self.parse_expression();
                self.lexer.match_token(Token::Semicolon);
                //statements.push(Statement::Declare(variable_name, value));
                println!("Finished parsing declaration");
            }
            // Is it a keyword?
            else if let Token::Identifier(name) = next_token {
                let statements_in_expr = match name.as_str() {
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
        let test = self.parse_expression();
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

        //println!("Found function: fn {function_name}() -> {return_type:?} {{{body:?}}}");

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
    use crate::parser::Expr::{
        AssignmentExpr, CallExpr, FloatExpr, IntExpr, NameExpr, OperatorExpr, PrefixExpr, TestExpr,
    };
    use crate::parser::{
        BlockStatement, Expr, IfStatement, InfixOperator, Parser, PrefixOperator,
        PrimitiveTypeName, ReturnStatement, Statement,
    };
    use alloc::boxed::Box;
    use alloc::{format, vec};

    fn assert_parse_expr_by_repr(source: &str, expected_repr: &str) {
        let mut parser = Parser::new(source);
        let parsed_expr = parser.parse_expression();
        assert_eq!(expected_repr, format!("{}", parsed_expr))
    }

    fn assert_parse_expr_by_tree(source: &str, expected_expr: Expr) {
        let mut parser = Parser::new(source);
        let parsed_expr = parser.parse_expression();
        assert_eq!(expected_expr, parsed_expr)
    }

    #[test]
    fn parse_prefix_expr() {
        let source = r"abc";
        assert_parse_expr_by_repr(source, "abc");
        assert_parse_expr_by_tree(source, NameExpr(Token::Identifier("abc".into())));

        let source = r"-+~!foo";
        assert_parse_expr_by_repr(source, "(-(+(~(!foo))))");
        assert_parse_expr_by_tree(
            source,
            PrefixExpr(
                PrefixOperator::Minus,
                Box::new(PrefixExpr(
                    PrefixOperator::Plus,
                    Box::new(PrefixExpr(
                        PrefixOperator::Tilde,
                        Box::new(PrefixExpr(
                            PrefixOperator::Bang,
                            Box::new(NameExpr(Token::Identifier("foo".into()))),
                        )),
                    )),
                )),
            ),
        );
    }

    #[test]
    fn parse_infix_expr() {
        let source = r"-foo + bar";
        assert_parse_expr_by_repr(source, "((-foo) + bar)");
        assert_parse_expr_by_tree(
            source,
            OperatorExpr(
                Box::new(PrefixExpr(
                    PrefixOperator::Minus,
                    Box::new(NameExpr(Token::Identifier("foo".into()))),
                )),
                InfixOperator::Plus,
                Box::new(NameExpr(Token::Identifier("bar".into()))),
            ),
        );
        assert_parse_expr_by_repr("a + b * c - d", "((a + (b * c)) - d)")
    }

    #[test]
    fn parse_call_expr() {
        assert_parse_expr_by_repr("a()", "a()");
        assert_parse_expr_by_repr("a(b)", "a(b)");
        assert_parse_expr_by_repr("a(b, c)", "a(b, c)");
        assert_parse_expr_by_repr("a(b)(c)", "a(b)(c)");
        assert_parse_expr_by_repr("a(b) + c(d)", "(a(b) + c(d))");
        assert_parse_expr_by_repr("foo(bar, baz + bad)", "foo(bar, (baz + bad))");
        assert_parse_expr_by_tree(
            "foo(bar, baz + bad)",
            CallExpr(
                Box::new(NameExpr(Token::Identifier("foo".into()))),
                vec![
                    NameExpr(Token::Identifier("bar".into())),
                    OperatorExpr(
                        Box::new(NameExpr(Token::Identifier("baz".into()))),
                        InfixOperator::Plus,
                        Box::new(NameExpr(Token::Identifier("bad".into()))),
                    ),
                ],
            ),
        );
        assert_parse_expr_by_repr("a(b ? c : d, e + f)", "a((b ? c : d), (e + f))");
    }

    #[test]
    fn parse_unary_binary_precedence() {
        assert_parse_expr_by_repr("-a * b", "((-a) * b)");
        assert_parse_expr_by_repr("!a + b", "((!a) + b)");
        assert_parse_expr_by_repr("~a ^ b", "((~a) ^ b)");
    }

    #[test]
    fn parse_binary_precedence() {
        assert_parse_expr_by_repr(
            "a = b + c * d ^ e - f / g",
            "(a = ((b + (c * (d ^ e))) - (f / g)))",
        );
    }

    #[test]
    fn parse_assignment() {
        assert_parse_expr_by_repr("foo = bar + baz", "(foo = (bar + baz))");
        assert_parse_expr_by_tree(
            "foo = bar + baz",
            AssignmentExpr(
                "foo".into(),
                Box::new(OperatorExpr(
                    Box::new(NameExpr(Token::Identifier("bar".into()))),
                    InfixOperator::Plus,
                    Box::new(NameExpr(Token::Identifier("baz".into()))),
                )),
            ),
        );
        assert_parse_expr_by_repr(
            "a = b + c * d ^ e - f / g",
            "(a = ((b + (c * (d ^ e))) - (f / g)))",
        );
    }

    #[test]
    fn parse_binary_associativity() {
        assert_parse_expr_by_repr("a = b = c", "(a = (b = c))");
        assert_parse_expr_by_repr("a + b - c", "((a + b) - c)");
        assert_parse_expr_by_repr("a * b / c", "((a * b) / c)");
        // Note that ^ is right-associative
        assert_parse_expr_by_repr("a ^ b ^ c", "(a ^ (b ^ c))");
    }

    #[test]
    fn parse_ternary_expr() {
        assert_parse_expr_by_repr("a ? b : c ? d : e", "(a ? b : (c ? d : e))");
        assert_parse_expr_by_repr("a ? b ? c : d : e", "(a ? (b ? c : d) : e)");
        assert_parse_expr_by_repr("a + b ? c * d : e / f", "((a + b) ? (c * d) : (e / f))");
    }

    #[test]
    fn parse_grouping() {
        assert_parse_expr_by_repr("a + (b + c) + d", "((a + (b + c)) + d)");
        assert_parse_expr_by_repr("a ^ (b + c)", "(a ^ (b + c))");
    }

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
            vec![Statement::Return(ReturnStatement::new(IntExpr(5)))]
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
            vec![Statement::Return(ReturnStatement::new(FloatExpr(9.12345)))]
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
                Statement::If(IfStatement::new(
                    TestExpr(Box::new(IntExpr(1)), Box::new(IntExpr(2)),),
                    BlockStatement::new(vec![Statement::Return(ReturnStatement::new(IntExpr(3))),])
                )),
                Statement::Return(ReturnStatement::new(IntExpr(4))),
            ]
        );
        function.validate_return_statements();
    }

    #[test]
    fn parse_plus() {
        let source = r"
        void f() {
            return 1 + 2;
        }";
        let mut parser = Parser::new(source);
        let function = parser.parse_function();
        assert_eq!(function.return_type, PrimitiveTypeName::Void);
        assert_eq!(function.name, "f");
        assert_eq!(
            function.body.statements,
            vec![Statement::Return(ReturnStatement::new(OperatorExpr(
                Box::new(IntExpr(1)),
                InfixOperator::Plus,
                Box::new(IntExpr(2))
            )))]
        );
        function.validate_return_statements();
    }
}
