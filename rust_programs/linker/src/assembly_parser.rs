use crate::{
    assembly_lexer::{AssemblyLexer, Token},
    assembly_packer::{DataSource, Instruction, Interrupt, MoveValueToRegister, Register},
    print, println,
    symbols::{DataSymbol, SymbolData, SymbolExpressionOperand},
};
use alloc::{collections::BTreeMap, fmt::Debug, rc::Rc, string::ToString, vec::Vec};
use alloc::{string::String, vec};
use cstr_core::CString;

#[derive(Debug, PartialEq)]
enum BinarySection {
    Text,
    ReadOnlyData,
}

#[derive(Debug)]
pub enum Expression {
    Subtract(SymbolExpressionOperand, SymbolExpressionOperand),
}

#[derive(Debug)]
pub enum AssemblyStatement {
    // Directives
    SetCurrentSection(String),
    Label(String),
    Ascii(String),
    Equ(String, Expression),
    // Instructions
    MoveImmediateIntoRegister(usize, Register),
    MoveSymbolIntoRegister(String, Register),
    MoveRegisterIntoRegister(Register, Register),
    Interrupt(u8),
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
                    _ => panic!("Unhandled {directive_name}"),
                }
            }
            Token::Identifier(name) => {
                // Is this a label declaration?
                if let Some(Token::Colon) = self.lexer.peek_token() {
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
                    _ => panic!("Unimplemented mnemonic {name}"),
                }
            }
            _ => self.parse_statement(),
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
            }
        }
    }

    pub fn parse(&mut self) -> (BTreeMap<String, Rc<DataSymbol>>, Vec<Rc<dyn Instruction>>) {
        println!("[### Parsing ###]");
        self.debug_parse();
        // Now, reset state and do the real parse
        self.lexer.reset();

        let mut instructions = vec![];
        let mut data_symbols = BTreeMap::new();

        let mut current_section = BinarySection::Text;
        let mut current_label = None;
        loop {
            let mut should_clear_current_label = true;
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
                        current_label = Some(name.clone());
                        // Don't clear the label until after the next statement
                        should_clear_current_label = false;
                    }
                    AssemblyStatement::MoveImmediateIntoRegister(immediate, register) => {
                        // TODO(PT): Consider lifting this restriction
                        // (Placing ascii data in .text is perfectly allowed by GAS, for example)
                        assert_eq!(current_section, BinarySection::Text);
                        instructions.push(Rc::new(MoveValueToRegister::new(register, DataSource::Literal(immediate))) as Rc<dyn Instruction>);
                    }
                    AssemblyStatement::MoveSymbolIntoRegister(symbol_name, register) => {
                        assert_eq!(current_section, BinarySection::Text);
                        instructions.push(Rc::new(MoveValueToRegister::new(register, DataSource::NamedDataSymbol(symbol_name.clone()))) as Rc<dyn Instruction>);
                    }
                    AssemblyStatement::MoveRegisterIntoRegister(source_register, register) => {
                        assert_eq!(current_section, BinarySection::Text);
                        instructions.push(Rc::new(MoveValueToRegister::new(register, DataSource::RegisterContents(source_register))) as Rc<dyn Instruction>);
                    }
                    AssemblyStatement::Interrupt(vector) => {
                        assert_eq!(current_section, BinarySection::Text);
                        instructions.push(Rc::new(Interrupt::new(vector)));
                    }
                    AssemblyStatement::Ascii(text) => {
                        assert_eq!(current_section, BinarySection::ReadOnlyData);
                        // TODO(PT): It should be possible to define .ascii without a directly preceding label
                        assert!(current_label.is_some());
                        let label_name = current_label.as_ref().unwrap();
                        data_symbols.insert(
                            label_name.clone(),
                            Rc::new(DataSymbol::new(
                                label_name,
                                SymbolData::LiteralData(CString::new(text.clone()).unwrap().into_bytes_with_nul()),
                            )),
                        );
                    }
                    AssemblyStatement::Equ(label_name, expression) => {
                        assert_eq!(current_section, BinarySection::ReadOnlyData);
                        match expression {
                            Expression::Subtract(op1, op2) => {
                                data_symbols.insert(label_name.clone(), Rc::new(DataSymbol::new(&label_name, SymbolData::Subtract((op1, op2)))));
                            }
                        };
                    }
                }

                // Erase the 'current label' as it should only apply to the statement directly after a label
                if should_clear_current_label {
                    current_label = None;
                }
            } else {
                break;
            }
        }
        (data_symbols, instructions)
    }
}
