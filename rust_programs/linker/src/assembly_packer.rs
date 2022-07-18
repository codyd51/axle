use alloc::{
    borrow::ToOwned,
    boxed::Box,
    collections::BTreeMap,
    rc::Rc,
    string::{String, ToString},
    vec,
};
use alloc::{slice, vec::Vec};
use bitflags::bitflags;
use core::{cell::RefCell, fmt::Display, mem, ops::Index};
use cstr_core::CString;

#[cfg(feature = "run_in_axle")]
use axle_rt::{print, println};
#[cfg(not(feature = "run_in_axle"))]
use std::{print, println};

use crate::{
    assembly_lexer::AssemblyLexer,
    assembly_parser::AssemblyParser,
    new_try::{FileLayout, MagicPackable, RebaseTarget, RebasedValue, SymbolEntryType},
    symbols::{DataSymbol, SymbolData, SymbolExpressionOperand},
};

enum RexPrefixOption {
    Use64BitOperandSize,
    UseRegisterFieldExtension,
    UseIndexFieldExtension,
    UseBaseFieldExtension,
}

struct RexPrefix;
impl RexPrefix {
    fn new(options: Vec<RexPrefixOption>) -> u8 {
        let mut out = (0b0100 << 4);
        for option in options.iter() {
            match option {
                RexPrefixOption::Use64BitOperandSize => out |= (1 << 3),
                RexPrefixOption::UseRegisterFieldExtension => out |= (1 << 2),
                RexPrefixOption::UseIndexFieldExtension => out |= (1 << 1),
                RexPrefixOption::UseBaseFieldExtension => out |= (1 << 0),
            }
        }
        out
    }

    fn for_64bit_operand() -> u8 {
        Self::new(vec![RexPrefixOption::Use64BitOperandSize])
    }
}

#[derive(Debug, Copy, Clone)]
enum Register {
    Rax,
    Rcx,
    Rdx,
    Rbx,
    Rsp,
    Rbp,
    Rsi,
    Rdi,
}

enum ModRmAddressingMode {
    RegisterDirect,
}

struct ModRmByte;
impl ModRmByte {
    fn register_index(register: Register) -> usize {
        match register {
            Register::Rax => 0b000,
            Register::Rcx => 0b001,
            Register::Rdx => 0b010,
            Register::Rbx => 0b011,
            Register::Rsp => 0b100,
            Register::Rbp => 0b101,
            Register::Rsi => 0b110,
            Register::Rdi => 0b111,
        }
    }
    fn new(addressing_mode: ModRmAddressingMode, register: Register, register2: Option<Register>) -> u8 {
        let mut out = 0;

        match addressing_mode {
            ModRmAddressingMode::RegisterDirect => out |= (0b11 << 6),
        }

        out |= Self::register_index(register);

        if let Some(register2) = register2 {
            out |= Self::register_index(register2) << 3;
        }

        out as _
    }
}

struct MoveDataSymbolToRegister {
    dest_register: Register,
    symbol: Rc<DataSymbol>,
}

impl MoveDataSymbolToRegister {
    fn new(dest_register: Register, symbol: &Rc<DataSymbol>) -> Self {
        Self {
            dest_register,
            symbol: Rc::clone(symbol),
        }
    }

    fn render(&self, layout: &FileLayout, rebased_value: &RebasedValue) -> Vec<u8> {
        let mut out = vec![];
        // REX prefix
        out.push(RexPrefix::for_64bit_operand());
        // MOV opcode
        out.push(0xc7);
        // Destination register
        out.push(ModRmByte::new(ModRmAddressingMode::RegisterDirect, self.dest_register, None));
        // Source memory operand
        let address = layout.get_rebased_value(*rebased_value) as u32;
        let mut address_bytes = address.to_le_bytes().to_owned().to_vec();
        out.append(&mut address_bytes);

        out
    }
}

#[derive(Debug, Clone)]
enum DataSource {
    Literal(usize),
    NamedDataSymbol(String),
    RegisterContents(Register),
    OutputCursor,
    Subtraction(Box<DataSource>, Box<DataSource>),
}

struct MoveValueToRegister {
    dest_register: Register,
    source: DataSource,
}

impl MoveValueToRegister {
    fn new(dest_register: Register, source: DataSource) -> Self {
        Self { dest_register, source }
    }
}

impl Instruction for MoveValueToRegister {
    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = vec![];
        // REX prefix
        out.push(RexPrefix::for_64bit_operand());

        if let DataSource::RegisterContents(register_name) = self.source {
            // MOV <reg>, <reg> opcode
            out.push(0x89);
            out.push(ModRmByte::new(ModRmAddressingMode::RegisterDirect, self.dest_register, Some(register_name)));
        } else {
            let value: u32 = match &self.source {
                DataSource::Literal(value) => *value as _,
                DataSource::NamedDataSymbol(symbol_name) => {
                    //println!("handling named symbol {symbol_name}");
                    let symbol_id = layout.get_symbol_id_for_symbol_name(&symbol_name);
                    let symbol_type = layout.get_symbol_entry_type_for_symbol_name(&symbol_name);
                    match symbol_type {
                        SymbolEntryType::SymbolWithBackingData => layout.get_rebased_value(RebasedValue::ReadOnlyDataSymbolVirtAddr(symbol_id)) as _,
                        SymbolEntryType::SymbolWithInlinedValue => layout.get_rebased_value(RebasedValue::ReadOnlyDataSymbolValue(symbol_id)) as _,
                    }
                }
                _ => panic!("Unexpected"),
            };
            // MOV <reg>, <mem> opcode
            out.push(0xc7);
            // Destination register
            out.push(ModRmByte::new(ModRmAddressingMode::RegisterDirect, self.dest_register, None));
            // Source value
            let mut value_bytes = value.to_le_bytes().to_owned().to_vec();
            out.append(&mut value_bytes);
        }

        out
    }
}

impl Display for MoveValueToRegister {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("mov {:?}, {:?}", self.dest_register, self.source))
    }
}

struct Interrupt {
    vector: u8,
}

impl Interrupt {
    fn new(vector: u8) -> Self {
        Self { vector }
    }
}

impl Instruction for Interrupt {
    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        vec![
            // INT opcode
            0xcd,
            // INT vector
            self.vector,
        ]
    }
}

impl Display for Interrupt {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("int 0x{:02x}", self.vector))
    }
}

trait Instruction: Display {
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
}

struct Function {
    name: String,
    instructions: Vec<MoveDataSymbolToRegister>,
}

impl Function {
    fn new(name: &str, instructions: Vec<MoveDataSymbolToRegister>) -> Self {
        Self {
            name: name.to_string(),
            instructions,
        }
    }
}

pub type SymbolOffset = usize;
pub type SymbolSize = usize;

pub struct DataPacker {
    file_layout: Rc<FileLayout>,
    pub symbols: Vec<(SymbolOffset, SymbolSize, Rc<DataSymbol>)>,
}

impl DataPacker {
    fn new(file_layout: &Rc<FileLayout>) -> Self {
        Self {
            file_layout: Rc::clone(file_layout),
            symbols: Vec::new(),
        }
    }

    fn total_size(&self) -> usize {
        /*
        let mut cursor = 0;
        for sym in self.symbols.iter() {
            cursor += sym.inner.len();
        }
        cursor
        */
        let last_entry = self.symbols.last();
        match last_entry {
            None => 0,
            Some(last_entry) => last_entry.0 + last_entry.1,
        }
    }

    pub fn pack(self_: &Rc<RefCell<Self>>, sym: &Rc<DataSymbol>) {
        let mut this = self_.borrow_mut();
        let total_size = this.total_size();
        //println!("Data packer tracking {sym}");
        this.symbols.push((total_size, sym.inner.len(), Rc::clone(sym)));
    }

    pub fn offset_of(self_: &Rc<RefCell<Self>>, sym: &Rc<DataSymbol>) -> usize {
        let this = self_.borrow();
        for (offset, _size, s) in this.symbols.iter() {
            if *s == *sym {
                return *offset;
            }
        }
        panic!("Failed to find symbol {sym}")
    }

    /*
    pub fn render(self: &Rc<Self>) -> Vec<u8> {
        let len = self.total_size();
        let mut out = vec![0; len];
        let mut cursor = 0;
        for sym in self.symbols.iter() {
            out[cursor..cursor + sym.inner.len()].copy_from_slice(&sym.inner);
        }
        out
    }
    */
}

pub struct InstructionPacker {
    file_layout: Rc<FileLayout>,
    data_packer: Rc<RefCell<DataPacker>>,
    instructions: RefCell<Vec<Rc<dyn Instruction>>>,
    rendered_instructions: RefCell<Vec<u8>>,
}

impl InstructionPacker {
    fn new(file_layout: &Rc<FileLayout>, data_packer: &Rc<RefCell<DataPacker>>) -> Self {
        Self {
            file_layout: Rc::clone(file_layout),
            data_packer: Rc::clone(data_packer),
            instructions: RefCell::new(Vec::new()),
            rendered_instructions: RefCell::new(Vec::new()),
        }
    }

    fn pack(&self, instr: &Rc<dyn Instruction>) {
        let mut instructions = self.instructions.borrow_mut();
        instructions.push(Rc::clone(instr));
    }
}

impl MagicPackable for InstructionPacker {
    fn len(&self) -> usize {
        self.rendered_instructions.borrow().len()
    }

    fn prerender(&self, layout: &FileLayout) {
        println!("Assembling code...");
        let mut rendered_instructions = self.rendered_instructions.borrow_mut();
        let instructions = self.instructions.borrow();
        for instr in instructions.iter() {
            let mut instruction_bytes = instr.render(layout);

            print!("\tAssembled \"{instr}\" to ");
            for byte in instruction_bytes.iter() {
                print!("{byte:02x}");
            }
            println!();

            rendered_instructions.append(&mut instruction_bytes);
        }
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        self.rendered_instructions.borrow().to_owned()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::TextSection
    }
}

pub fn parse(layout: &Rc<FileLayout>, source: &str) -> (Rc<RefCell<DataPacker>>, Rc<InstructionPacker>) {
    let lexer = AssemblyLexer::new(source);
    let mut parser = AssemblyParser::new(lexer);
    parser.parse();
    /*
    loop {
        let token = lexer.next_token();
        match token {
            Some(_) => println!("{:?}", token.unwrap()),
            None => break,
        }
    }
    */
    panic!("done");

    // Generate code and data
    // TODO(PT): Eventually, this will be an assembler stage
    let mut data_symbols = BTreeMap::new();

    let string = Rc::new(DataSymbol::new(
        "msg",
        SymbolData::LiteralData(CString::new("Hello world!\n").unwrap().into_bytes_with_nul()),
    ));
    let string_len = Rc::new(DataSymbol::new(
        "msg_len",
        SymbolData::Subtract((SymbolExpressionOperand::OutputCursor, SymbolExpressionOperand::StartOfSymbol("msg".to_string()))),
    ));
    data_symbols.insert(string.name.clone(), Rc::clone(&string));
    data_symbols.insert(string_len.name.clone(), Rc::clone(&string_len));

    // Render the data symbols
    let mut data_packer = Rc::new(RefCell::new(DataPacker::new(layout)));
    for data_symbol in data_symbols.values() {
        DataPacker::pack(&data_packer, &data_symbol);
    }

    let instructions = vec![
        // Syscall vector (_write)
        Rc::new(MoveValueToRegister::new(Register::Rax, DataSource::Literal(0xc))) as Rc<dyn Instruction>,
        // File descriptor (stdout)
        Rc::new(MoveValueToRegister::new(Register::Rbx, DataSource::Literal(0x1))) as Rc<dyn Instruction>,
        // String pointer
        Rc::new(MoveValueToRegister::new(Register::Rcx, DataSource::NamedDataSymbol(string.name.clone()))) as Rc<dyn Instruction>,
        // String length
        Rc::new(MoveValueToRegister::new(Register::Rdx, DataSource::NamedDataSymbol(string_len.name.clone()))) as Rc<dyn Instruction>,
        // Syscall
        Rc::new(Interrupt::new(0x80)) as Rc<dyn Instruction>,
        // _exit status code is the write() retval (# bytes written)
        Rc::new(MoveValueToRegister::new(Register::Rbx, DataSource::RegisterContents(Register::Rax))) as Rc<dyn Instruction>,
        // _exit syscall vector
        Rc::new(MoveValueToRegister::new(Register::Rax, DataSource::Literal(0xd))) as Rc<dyn Instruction>,
        // Syscall
        Rc::new(Interrupt::new(0x80)) as Rc<dyn Instruction>,
    ];

    // Render the instructions
    let mut instruction_packer = Rc::new(InstructionPacker::new(&layout, &data_packer));
    for instr in instructions.iter() {
        instruction_packer.pack(instr);
    }

    (data_packer, instruction_packer)
}
