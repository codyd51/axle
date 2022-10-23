use core::{cell::RefCell, fmt::Display, mem};
use alloc::{fmt::Debug, rc::Rc, string::ToString, vec::Vec};
use alloc::{string::String, vec};
use cstr_core::CString;
use compilation_definitions::instructions::{AddRegToReg, CompareImmWithReg, DivRegByReg, Instr, MoveImmToReg, MoveImmToRegMemOffset, MoveRegToReg, MulRegByReg, SubRegFromReg};

use compilation_definitions::prelude::*;
use compilation_definitions::asm::{AsmExpr, SymbolExprOperand};

use crate::{
    assembly_lexer::{AssemblyLexer, Token},
    assembly_packer::{DataSource, Interrupt, Jump, JumpTarget, MoveValueToRegister, PotentialLabelTarget},
    print, println,
    symbols::{ConstantData, SymbolData},
};
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
    pub expression: AsmExpr,
    pub previous_data_unit: RefCell<Option<Rc<dyn PotentialLabelTarget>>>,
}

impl EquExpression {
    fn new(container_section: BinarySection, name: &str, expression: AsmExpr, previous_atom: &Option<Rc<dyn PotentialLabelTarget>>) -> Self {
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

    fn register_from_str(&mut self, reg_str: &str) -> RegView {
        match reg_str {
            "rax" => RegView::rax(),
            "rcx" => RegView::rcx(),
            "rdx" => RegView::rdx(),
            "rbx" => RegView::rbx(),
            "rsp" => RegView::rsp(),
            "rbp" => RegView::rbp(),
            "rsi" => RegView::rsi(),
            "rdi" => RegView::rdi(),
            _ => panic!("Unexpected register name {reg_str}"),
        }
    }

    fn match_register(&mut self) -> RegView {
        let register_name = self.match_identifier();
        self.register_from_str(&register_name)
    }

    fn int_from_hex_string(&self, s: &str) -> usize {
        let trimmed_immediate = s.trim_start_matches("0x");
        usize::from_str_radix(trimmed_immediate, 16).unwrap()
    }

    pub fn parse_statement(&mut self) -> Option<Instr> {
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
                        Some(Instr::DirectiveSetCurrentSection(section_name))
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
                        Some(Instr::DirectiveEmbedAscii(literal_string_data))
                    }
                    "equ" => {
                        let label_name = self.match_identifier();
                        self.match_token(Token::Comma);
                        // TODO(PT): Expand to handle more generic expressions
                        self.match_token(Token::Dot);
                        self.match_token(Token::Minus);
                        let op2_name = self.match_identifier();
                        Some(Instr::DirectiveEqu(
                            label_name,
                            AsmExpr::Subtract(
                                SymbolExprOperand::OutputCursor,
                                SymbolExprOperand::StartOfSymbol(op2_name)
                            ),
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
                        Some(Instr::DirectiveEmbedU32(immediate as _))
                    }
                    _ => panic!("Unhandled {directive_name}"),
                }
            }
            Token::Identifier(name) => {
                // Is this a label declaration?
                if let Some(Token::Colon) = self.lexer.peek_token() {
                    // Consume the colon
                    self.match_token(Token::Colon);
                    return Some(Instr::DirectiveDeclareLabel(name));
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
                                    Some(Instr::MoveImmToReg(MoveImmToReg::new(immediate, dest_register)))
                                } else {
                                    // Named symbol
                                    //Some(Instr::MoveSymbolToReg(source, dest_register))
                                    todo!()
                                }
                            }
                            Token::Percent => {
                                // Register
                                let source_register = self.register_from_str(&source);
                                Some(Instr::MoveRegToReg(MoveRegToReg::new(source_register, dest_register)))
                            }
                            _ => panic!("Unexpected leading symbol"),
                        }
                    }
                    "int" => {
                        self.match_token(Token::Dollar);
                        let interrupt_vector_str = self.match_identifier();
                        let interrupt_vector = self.int_from_hex_string(&interrupt_vector_str);
                        Some(Instr::Interrupt(interrupt_vector as u8))
                    }
                    "jmp" => {
                        // TODO(PT): The leading dollar here is non-standard syntax
                        self.match_token(Token::Dollar);
                        let source = self.match_identifier();
                        // TODO(PT): For now, we only support named symbols as jump targets
                        Some(Instr::JumpToLabel(source))
                    }
                    "push" => {
                        self.match_token(Token::Percent);
                        let register = self.match_register();
                        Some(Instr::PushFromReg(register))
                    }
                    "pop" => {
                        self.match_token(Token::Percent);
                        let register = self.match_register();
                        Some(Instr::PopIntoReg(register))
                    }
                    "add" => {
                        self.match_token(Token::Percent);
                        let augend = self.match_register();
                        self.match_token(Token::Comma);
                        self.match_token(Token::Percent);
                        let addend = self.match_register();
                        Some(Instr::AddRegToReg(AddRegToReg::new(augend, addend)))
                    }
                    "ret" => {
                        Some(Instr::Return)
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
                Instr::DirectiveSetCurrentSection(name) => {
                    match name.as_str() {
                        "text" => current_section = BinarySection::Text,
                        "rodata" => current_section = BinarySection::ReadOnlyData,
                        _ => panic!("Unknown name {name}"),
                    };
                    println!("[OutputSection = {current_section:?}]");
                }
                Instr::DirectiveDeclareLabel(name) => {
                    println!("[Label {name}]");
                }
                Instr::DirectiveEmbedAscii(text) => {
                    let escaped_text = text.replace('\n', "\\n");
                    println!("[LiteralAscii \"{escaped_text}\"]");
                }
                Instr::DirectiveEqu(label_name, expression) => {
                    let format_operand = |op| match op {
                        SymbolExprOperand::OutputCursor => ".".to_string(),
                        SymbolExprOperand::StartOfSymbol(sym) => sym,
                    };
                    print!("[Equ {label_name} = ");
                    match expression {
                        AsmExpr::Subtract(op1, op2) => {
                            println!("{} - {}]", format_operand(op1), format_operand(op2));
                        }
                    };
                }
                Instr::DirectiveEmbedU32(immediate) => {
                    println!("[LiteralWord {immediate}]");
                }
                Instr::MoveImmToReg(MoveImmToReg { imm, dest }) => {
                    println!("[Move {imm:016x} => {dest:?}]");
                }
                /*
                Instr::MoveSymbolToReg(symbol_name, register) => {
                    println!("[Move ${symbol_name} => {register:?}]");
                }
                */
                Instr::MoveRegToReg(MoveRegToReg { source, dest }) => {
                    println!("[Move {source:?} => {dest:?}]");
                }
                Instr::Interrupt(vector) => {
                    println!("[Int 0x{vector:02x}]");
                }
                Instr::JumpToLabel(label_name) => {
                    println!("[Jump {label_name}]");
                }
                Instr::PushFromReg(reg) => {
                    println!("[Push {reg:?}]");
                }
                Instr::PopIntoReg(reg) => {
                    println!("[Pop {reg:?}]");
                }
                Instr::AddRegToReg(AddRegToReg { augend, addend }) => {
                    println!("[Add {augend:?}, {addend:?}]");
                }
                Instr::Return => {
                    println!("[Ret]");
                }
                Instr::DirectiveDeclareGlobalSymbol(symbol) => {
                    println!("[Global {symbol}");
                }
                Instr::MoveImmToRegMemOffset(MoveImmToRegMemOffset { imm, offset, reg_to_deref }) => {
                    println!("[Move imm => [{reg_to_deref:?} + {offset}]");
                }
                Instr::NegateRegister(reg) => {
                    println!("[Negate {reg:?}]");
                }
                Instr::SubRegFromReg(SubRegFromReg { minuend, subtrahend }) => {
                    println!("[Subtract {minuend:?} - {subtrahend:?}]");
                }
                Instr::MulRegByReg(MulRegByReg { multiplicand, multiplier }) => {
                    println!("[Multiply {multiplicand:?} * {multiplier:?}]");
                }
                Instr::DivRegByReg(DivRegByReg { dividend, divisor }) => {
                    println!("[Divide {dividend:?} * {divisor:?}]");
                }
                Instr::JumpToLabelIfEqual(label) => {
                    println!("[JumpIfEqual {label}");
                }
                Instr::CompareImmWithReg(CompareImmWithReg { imm, reg }) => {
                    println!("Compare {imm} with {reg:?}");
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
                    Instr::DirectiveSetCurrentSection(name) => {
                        match name.as_str() {
                            "text" => current_section = BinarySection::Text,
                            "rodata" => current_section = BinarySection::ReadOnlyData,
                            _ => panic!("Unknown name {name}"),
                        };
                    }
                    Instr::DirectiveDeclareLabel(name) => {
                        *maybe_current_label.borrow_mut() = Some(Label::new(current_section, &name));
                    }
                    Instr::DirectiveEmbedAscii(text) => {
                        append_data_unit(Rc::new(ConstantData::new(
                            current_section,
                            SymbolData::LiteralData(CString::new(text.clone()).unwrap().into_bytes_with_nul()),
                        )));
                    }
                    Instr::DirectiveEqu(label_name, expression) => {
                        equ_expressions.push(Rc::new(EquExpression::new(current_section, &label_name, expression, &previous_atom.borrow())));
                    }
                    Instr::DirectiveEmbedU32(immediate) => {
                        // Make sure this is exactly 4 bytes
                        let mut word_bytes = immediate.to_le_bytes().to_vec();
                        word_bytes.resize(mem::size_of::<u32>(), 0);
                        append_data_unit(Rc::new(ConstantData::new(current_section, SymbolData::LiteralData(word_bytes))));
                    }
                    Instr::MoveImmToReg(MoveImmToReg { imm, dest }) => {
                        // TODO(PT): Rename to Atom?
                        append_data_unit(Rc::new(MoveValueToRegister::new(dest, DataSource::Literal(imm))) as Rc<dyn PotentialLabelTarget>);
                    }
                    /*
                    Instr::MoveSymbolToRegister(symbol_name, register) => {
                        append_data_unit(
                            Rc::new(MoveValueToRegister::new(register, DataSource::NamedDataSymbol(symbol_name.clone()))) as Rc<dyn PotentialLabelTarget>
                        );
                    }
                    */
                    Instr::MoveRegToReg(MoveRegToReg { source, dest }) => {
                        append_data_unit(
                            Rc::new(MoveValueToRegister::new(dest, DataSource::RegisterContents(source))) as Rc<dyn PotentialLabelTarget>
                        );
                    }
                    Instr::Interrupt(vector) => {
                        append_data_unit(Rc::new(Interrupt::new(vector)));
                    }
                    Instr::JumpToLabel(label_name) => {
                        append_data_unit(Rc::new(Jump::new(JumpTarget::Label(label_name))));
                    }
                    Instr::PushFromReg(reg) => {
                        append_data_unit(Rc::new(Push::new(reg)));
                    }
                    Instr::PopIntoReg(reg) => {
                        append_data_unit(Rc::new(Pop::new(reg)));
                    }
                    Instr::AddRegToReg(AddRegToReg { augend, addend }) => {
                        append_data_unit(Rc::new(Add::new(augend, addend)));
                    }
                    Instr::Return => {
                        append_data_unit(Rc::new(Ret::new()));
                    }
                    Instr::DirectiveDeclareGlobalSymbol(symbol_name) => {
                        todo!()
                    }
                    Instr::MoveImmToRegMemOffset(MoveImmToRegMemOffset { imm, offset, reg_to_deref }) => {
                        todo!()
                    }
                    Instr::NegateRegister(register) => {
                        todo!()
                    }
                    Instr::SubRegFromReg(SubRegFromReg { minuend, subtrahend }) => {
                        todo!()
                    }
                    Instr::MulRegByReg(MulRegByReg { multiplicand, multiplier }) => {
                        todo!()
                    }
                    Instr::DivRegByReg(DivRegByReg { dividend, divisor }) => {
                        todo!()
                    }
                    Instr::JumpToLabelIfEqual(label) => {
                        todo!()
                    }
                    Instr::CompareImmWithReg(CompareImmWithReg { imm, reg }) => {
                        todo!()
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
