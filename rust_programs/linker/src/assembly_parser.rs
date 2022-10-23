use core::{cell::RefCell, fmt::Display, mem};

use crate::{
    assembly_lexer::{AssemblyLexer, Token},
    assembly_packer::{DataSource, Interrupt, Jump, JumpTarget, MoveValueToRegister, PotentialLabelTarget, Register},
    print, println,
    symbols::{ConstantData, SymbolData, SymbolExpressionOperand},
};
use alloc::{fmt::Debug, rc::Rc, string::ToString, vec::Vec};
use alloc::{string::String, vec};
use cstr_core::CString;
use crate::assembly_packer::{Add, Pop, Push, Ret};

#[derive(Clone)]
pub struct Label {
    container_section: BinarySection,
    pub name: String,
    pub data_unit: RefCell<Option<Rc<dyn PotentialLabelTarget>>>,
}

impl Label {
    fn new(container_section: BinarySection, name: &str) -> Self {
        Self {
            container_section,
            name: name.to_string(),
            data_unit: RefCell::new(None),
        }
    }

    fn set_data_unit(&self, data_unit: &Rc<dyn PotentialLabelTarget>) {
        let mut maybe_data_unit = self.data_unit.borrow_mut();
        assert!(maybe_data_unit.is_none());
        *maybe_data_unit = Some(Rc::clone(data_unit))
    }
}

impl Display for Label {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "<Label in {}: {}", self.container_section, self.name)?;
        let data_unit = self.data_unit.borrow();
        if let Some(data_unit) = &*data_unit {
            write!(f, " => {data_unit}")?;
        }
        write!(f, ">")
    }
}

#[derive(Clone)]
pub struct EquExpression {
    container_section: BinarySection,
    pub name: String,
    pub expression: Expression,
    pub previous_data_unit: RefCell<Option<Rc<dyn PotentialLabelTarget>>>,
}

impl EquExpression {
    fn new(container_section: BinarySection, name: &str, expression: Expression, previous_atom: &Option<Rc<dyn PotentialLabelTarget>>) -> Self {
        Self {
            container_section,
            name: name.to_string(),
            expression,
            previous_data_unit: RefCell::new(match previous_atom {
                Some(x) => Some(x.clone()),
                None => None,
            }),
        }
    }
}

impl Display for EquExpression {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "<Equ in {}: {} => {}>", self.container_section, self.name, self.expression)
    }
}

#[derive(Debug, PartialEq, Copy, Clone)]
pub enum BinarySection {
    Text,
    ReadOnlyData,
}

impl Display for BinarySection {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                BinarySection::Text => "Text",
                BinarySection::ReadOnlyData => "ReadOnlyData",
            }
        )
    }
}

#[derive(Debug, Clone)]
pub enum Expression {
    Subtract(SymbolExpressionOperand, SymbolExpressionOperand),
}

impl Display for Expression {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Expression::Subtract(op1, op2) => write!(f, "{op1} - {op2}"),
        }
    }
}

// TODO(PT): Replace with Instruction
#[derive(Debug)]
pub enum AssemblyStatement {
    // Directives
    SetCurrentSection(String),
    Label(String),
    Ascii(String),
    LiteralWord(u32),
    Equ(String, Expression),
    // Instructions
    MoveImmediateIntoRegister(usize, Register),
    MoveSymbolIntoRegister(String, Register),
    MoveRegisterIntoRegister(Register, Register),
    Interrupt(u8),
    Jump(String),
    Push(Register),
    Pop(Register),
    Add(Register, Register),
    Ret,
}

pub struct AssemblyParser {
    lexer: AssemblyLexer,
}

impl AssemblyParser {
    pub fn new(lexer: AssemblyLexer) -> Self {
        Self { lexer }
    }

    fn match_identifier(&mut self) -> String {
        match self.lexer.next_token().unwrap() {
            Token::Identifier(name) => name,
            tok => panic!("Expected an identifier, found {tok:?}"),
        }
    }

    fn match_token(&mut self, expected_token: Token) {
        let actual_token = self.lexer.next_token().unwrap();
        assert_eq!(expected_token, actual_token);
    }

    fn register_from_str(&mut self, reg_str: &str) -> Register {
        match reg_str {
            "rax" => Register::Rax,
            "rcx" => Register::Rcx,
            "rdx" => Register::Rdx,
            "rbx" => Register::Rbx,
            "rsp" => Register::Rsp,
            "rbp" => Register::Rbp,
            "rsi" => Register::Rsi,
            "rdi" => Register::Rdi,
            _ => panic!("Unexpected register name {reg_str}"),
        }
    }

    fn match_register(&mut self) -> Register {
        let register_name = self.match_identifier();
        self.register_from_str(&register_name)
    }

    fn int_from_hex_string(&self, s: &str) -> usize {
        let trimmed_immediate = s.trim_start_matches("0x");
        usize::from_str_radix(trimmed_immediate, 16).unwrap()
    }

    //pub fn parse_statement(&mut self) -> Option<Box<dyn AssemblyStatement>> {
    pub fn parse_statement(&mut self) -> Option<AssemblyStatement> {
        let token = self.lexer.next_token()?;
        //println!("Token: {token:?}");

        // Label definitions need lookahead

        match token {
            Token::Dot => {
                // Parse a directive
                let directive_name = self.match_identifier();
                match directive_name.as_str() {
                    "section" => {
                        self.match_token(Token::Dot);
                        let section_name = self.match_identifier();
                        Some(AssemblyStatement::SetCurrentSection(section_name))
                    }
                    "global" => {
                        // Next is the symbol name
                        let _symbol_name = self.match_identifier();
                        //println!("Ignoring .global {_symbol_name}");
                        // Ignore it
                        self.parse_statement()
                    }
                    "ascii" => {
                        self.match_token(Token::Quote);
                        let literal_string_data = self.lexer.read_to('"');
                        self.match_token(Token::Quote);
                        Some(AssemblyStatement::Ascii(literal_string_data))
                    }
                    "equ" => {
                        let label_name = self.match_identifier();
                        self.match_token(Token::Comma);
                        // TODO(PT): Expand to handle more generic expressions
                        self.match_token(Token::Dot);
                        self.match_token(Token::Minus);
                        let op2_name = self.match_identifier();
                        Some(AssemblyStatement::Equ(
                            label_name,
                            Expression::Subtract(SymbolExpressionOperand::OutputCursor, SymbolExpressionOperand::StartOfSymbol(op2_name)),
                        ))
                    }
                    "word" => {
                        self.match_token(Token::Dollar);
                        let value = self.match_identifier();
                        let immediate = if value.starts_with("0x") {
                            // Hexadecimal immediate
                            self.int_from_hex_string(&value)
                        } else {
                            // Decimal immediate
                            value.parse().unwrap()
                        };
                        Some(AssemblyStatement::LiteralWord(immediate as _))
                    }
                    _ => panic!("Unhandled {directive_name}"),
                }
            }
            Token::Identifier(name) => {
                // Is this a label declaration?
                if let Some(Token::Colon) = self.lexer.peek_token() {
                    // Consume the colon
                    self.match_token(Token::Colon);
                    return Some(AssemblyStatement::Label(name));
                }
                match name.as_ref() {
                    "mov" => {
                        // Move source into register
                        let leading_symbol = self.lexer.next_token().unwrap();
                        let source = self.match_identifier();
                        self.match_token(Token::Comma);
                        self.match_token(Token::Percent);
                        let dest_register = self.match_register();

                        match leading_symbol {
                            Token::Dollar => {
                                if source.starts_with("0x") {
                                    // Hexadecimal immediate
                                    let immediate = self.int_from_hex_string(&source);
                                    Some(AssemblyStatement::MoveImmediateIntoRegister(immediate, dest_register))
                                } else {
                                    // Named symbol
                                    Some(AssemblyStatement::MoveSymbolIntoRegister(source, dest_register))
                                }
                            }
                            Token::Percent => {
                                // Register
                                let source_register = self.register_from_str(&source);
                                Some(AssemblyStatement::MoveRegisterIntoRegister(source_register, dest_register))
                            }
                            _ => panic!("Unexpected leading symbol"),
                        }
                    }
                    "int" => {
                        self.match_token(Token::Dollar);
                        let interrupt_vector_str = self.match_identifier();
                        let interrupt_vector = self.int_from_hex_string(&interrupt_vector_str);
                        Some(AssemblyStatement::Interrupt(interrupt_vector as u8))
                    }
                    "jmp" => {
                        // TODO(PT): The leading dollar here is non-standard syntax
                        self.match_token(Token::Dollar);
                        let source = self.match_identifier();
                        // TODO(PT): For now, we only support named symbols as jump targets
                        Some(AssemblyStatement::Jump(source))
                    }
                    "push" => {
                        self.match_token(Token::Percent);
                        let register = self.match_register();
                        Some(AssemblyStatement::Push(register))
                    }
                    "pop" => {
                        self.match_token(Token::Percent);
                        let register = self.match_register();
                        Some(AssemblyStatement::Pop(register))
                    }
                    "add" => {
                        self.match_token(Token::Percent);
                        let augend = self.match_register();
                        self.match_token(Token::Comma);
                        self.match_token(Token::Percent);
                        let addend = self.match_register();
                        Some(AssemblyStatement::Add(augend, addend))
                    }
                    "ret" => {
                        Some(AssemblyStatement::Ret)
                    }
                    _ => panic!("Unimplemented mnemonic {name}"),
                }
            }
            _ => panic!("Unexpected token {token:?}"),
        }
    }

    pub fn debug_parse(&mut self) {
        let mut current_section;
        while let Some(statement) = self.parse_statement() {
            match statement {
                AssemblyStatement::SetCurrentSection(name) => {
                    match name.as_str() {
                        "text" => current_section = BinarySection::Text,
                        "rodata" => current_section = BinarySection::ReadOnlyData,
                        _ => panic!("Unknown name {name}"),
                    };
                    println!("[OutputSection = {current_section:?}]");
                }
                AssemblyStatement::Label(name) => {
                    println!("[Label {name}]");
                }
                AssemblyStatement::Ascii(text) => {
                    let escaped_text = text.replace('\n', "\\n");
                    println!("[LiteralAscii \"{escaped_text}\"]");
                }
                AssemblyStatement::Equ(label_name, expression) => {
                    let format_operand = |op| match op {
                        SymbolExpressionOperand::OutputCursor => ".".to_string(),
                        SymbolExpressionOperand::StartOfSymbol(sym) => sym,
                    };
                    print!("[Equ {label_name} = ");
                    match expression {
                        Expression::Subtract(op1, op2) => {
                            println!("{} - {}]", format_operand(op1), format_operand(op2));
                        }
                    };
                }
                AssemblyStatement::LiteralWord(immediate) => {
                    println!("[LiteralWord {immediate}]");
                }
                AssemblyStatement::MoveImmediateIntoRegister(immediate, register) => {
                    println!("[Move {immediate:016x} => {register:?}]");
                }
                AssemblyStatement::MoveSymbolIntoRegister(symbol_name, register) => {
                    println!("[Move ${symbol_name} => {register:?}]");
                }
                AssemblyStatement::MoveRegisterIntoRegister(source_register, register) => {
                    println!("[Move {source_register:?} => {register:?}]");
                }
                AssemblyStatement::Interrupt(vector) => {
                    println!("[Int 0x{vector:02x}]");
                }
                AssemblyStatement::Jump(label_name) => {
                    println!("[Jump {label_name}]");
                }
                AssemblyStatement::Push(reg) => {
                    println!("[Push {reg:?}]");
                }
                AssemblyStatement::Pop(reg) => {
                    println!("[Pop {reg:?}]");
                }
                AssemblyStatement::Add(augend, addend) => {
                    println!("[Add {augend:?}, {addend:?}]");
                }
                AssemblyStatement::Ret => {
                    println!("[Ret]");
                }
            }
        }
    }

    pub fn parse(&mut self) -> (Labels, EquExpressions, PotentialLabelTargets) {
        println!("[### Parsing ###]");
        self.debug_parse();
        // Now, reset state and do the real parse
        self.lexer.reset();

        let mut labels = vec![];
        let mut data_units: Vec<Rc<dyn PotentialLabelTarget>> = vec![];
        let mut equ_expressions = vec![];

        let mut current_section = BinarySection::Text;
        let maybe_current_label: RefCell<Option<Label>> = RefCell::new(None);
        let previous_atom: RefCell<Option<Rc<dyn PotentialLabelTarget>>> = RefCell::new(None);

        let mut append_data_unit = |data_unit| {
            let mut maybe_current_label = maybe_current_label.borrow_mut();
            if let Some(current_label) = &*maybe_current_label {
                current_label.set_data_unit(&data_unit);
                labels.push(current_label.clone());
                *maybe_current_label = None;
            }
            let mut maybe_previous_atom = previous_atom.borrow_mut();
            *maybe_previous_atom = Some(Rc::clone(&data_unit));
            data_units.push(data_unit);
        };
        loop {
            if let Some(statement) = self.parse_statement() {
                match statement {
                    AssemblyStatement::SetCurrentSection(name) => {
                        match name.as_str() {
                            "text" => current_section = BinarySection::Text,
                            "rodata" => current_section = BinarySection::ReadOnlyData,
                            _ => panic!("Unknown name {name}"),
                        };
                    }
                    AssemblyStatement::Label(name) => {
                        *maybe_current_label.borrow_mut() = Some(Label::new(current_section, &name));
                    }
                    AssemblyStatement::Ascii(text) => {
                        append_data_unit(Rc::new(ConstantData::new(
                            current_section,
                            SymbolData::LiteralData(CString::new(text.clone()).unwrap().into_bytes_with_nul()),
                        )));
                    }
                    AssemblyStatement::Equ(label_name, expression) => {
                        equ_expressions.push(Rc::new(EquExpression::new(current_section, &label_name, expression, &previous_atom.borrow())));
                    }
                    AssemblyStatement::LiteralWord(immediate) => {
                        // Make sure this is exactly 4 bytes
                        let mut word_bytes = immediate.to_le_bytes().to_vec();
                        word_bytes.resize(mem::size_of::<u32>(), 0);
                        append_data_unit(Rc::new(ConstantData::new(current_section, SymbolData::LiteralData(word_bytes))));
                    }
                    AssemblyStatement::MoveImmediateIntoRegister(immediate, register) => {
                        // TODO(PT): Rename to Atom?
                        append_data_unit(Rc::new(MoveValueToRegister::new(register, DataSource::Literal(immediate))) as Rc<dyn PotentialLabelTarget>);
                    }
                    AssemblyStatement::MoveSymbolIntoRegister(symbol_name, register) => {
                        append_data_unit(
                            Rc::new(MoveValueToRegister::new(register, DataSource::NamedDataSymbol(symbol_name.clone()))) as Rc<dyn PotentialLabelTarget>
                        );
                    }
                    AssemblyStatement::MoveRegisterIntoRegister(source_register, register) => {
                        append_data_unit(
                            Rc::new(MoveValueToRegister::new(register, DataSource::RegisterContents(source_register))) as Rc<dyn PotentialLabelTarget>
                        );
                    }
                    AssemblyStatement::Interrupt(vector) => {
                        append_data_unit(Rc::new(Interrupt::new(vector)));
                    }
                    AssemblyStatement::Jump(label_name) => {
                        append_data_unit(Rc::new(Jump::new(JumpTarget::Label(label_name))));
                    }
                    AssemblyStatement::Push(reg) => {
                        append_data_unit(Rc::new(Push::new(reg)));
                    }
                    AssemblyStatement::Pop(reg) => {
                        append_data_unit(Rc::new(Pop::new(reg)));
                    }
                    AssemblyStatement::Add(augend, addend) => {
                        append_data_unit(Rc::new(Add::new(augend, addend)));
                    }
                    AssemblyStatement::Ret => {
                        append_data_unit(Rc::new(Ret::new()));
                    }
                }
            } else {
                break;
            }
        }
        (Labels(labels), EquExpressions(equ_expressions), PotentialLabelTargets(data_units))
    }
}

#[derive(Clone)]
pub struct Labels(pub Vec<Label>);

impl Display for Labels {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        self.0.iter().fold(Ok(()), |result, label| result.and_then(|_| writeln!(f, "{}", label)))
    }
}

#[derive(Clone)]
pub struct EquExpressions(pub Vec<Rc<EquExpression>>);

impl Display for EquExpressions {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        self.0.iter().fold(Ok(()), |result, equ_expr| result.and_then(|_| writeln!(f, "{}", equ_expr)))
    }
}

#[derive(Clone)]
pub struct PotentialLabelTargets(pub Vec<Rc<dyn PotentialLabelTarget>>);

impl Display for PotentialLabelTargets {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {
        self.0.iter().fold(Ok(()), |result, atom| result.and_then(|_| writeln!(f, "{}", atom)))
    }
}
