use alloc::{borrow::ToOwned, boxed::Box, rc::Rc, string::String, vec};
use alloc::{collections::BTreeMap, vec::Vec};
use core::{cell::RefCell, fmt::Display};

#[cfg(feature = "run_in_axle")]
use axle_rt::{print, println};
#[cfg(not(feature = "run_in_axle"))]
use std::{print, println};

use crate::{
    assembly_lexer::AssemblyLexer,
    assembly_parser::AssemblyParser,
    new_try::{FileLayout, MagicPackable, RebaseTarget, RebasedValue, SymbolEntryType},
    symbols::{DataSymbol, InstructionId},
};

enum RexPrefixOption {
    Use64BitOperandSize,
    _UseRegisterFieldExtension,
    _UseIndexFieldExtension,
    _UseBaseFieldExtension,
}

struct RexPrefix;
impl RexPrefix {
    fn from_options(options: Vec<RexPrefixOption>) -> u8 {
        let mut out = 0b0100 << 4;
        for option in options.iter() {
            match option {
                RexPrefixOption::Use64BitOperandSize => out |= 1 << 3,
                RexPrefixOption::_UseRegisterFieldExtension => out |= 1 << 2,
                RexPrefixOption::_UseIndexFieldExtension => out |= 1 << 1,
                RexPrefixOption::_UseBaseFieldExtension => out |= 1 << 0,
            }
        }
        out
    }

    fn for_64bit_operand() -> u8 {
        Self::from_options(vec![RexPrefixOption::Use64BitOperandSize])
    }
}

#[derive(Debug, Copy, Clone)]
pub enum Register {
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
    fn from(addressing_mode: ModRmAddressingMode, register: Register, register2: Option<Register>) -> u8 {
        let mut out = 0;

        match addressing_mode {
            ModRmAddressingMode::RegisterDirect => out |= 0b11 << 6,
        }

        out |= Self::register_index(register);

        if let Some(register2) = register2 {
            out |= Self::register_index(register2) << 3;
        }

        out as _
    }
}

#[derive(Debug, Clone)]
pub enum DataSource {
    Literal(usize),
    NamedDataSymbol(String),
    RegisterContents(Register),
    OutputCursor,
    Subtraction(Box<DataSource>, Box<DataSource>),
}

static mut NEXT_INSTRUCTION_ID: usize = 0;

fn next_instruction_id() -> InstructionId {
    unsafe {
        let ret = NEXT_INSTRUCTION_ID;
        NEXT_INSTRUCTION_ID += 1;
        InstructionId(ret)
    }
}

pub struct MoveValueToRegister {
    id: InstructionId,
    dest_register: Register,
    source: DataSource,
}

impl MoveValueToRegister {
    pub fn new(dest_register: Register, source: DataSource) -> Self {
        Self {
            id: next_instruction_id(),
            dest_register,
            source,
        }
    }
}

impl Instruction for MoveValueToRegister {
    fn id(&self) -> InstructionId {
        self.id
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        // REX prefix
        let mut out = vec![RexPrefix::for_64bit_operand()];

        if let DataSource::RegisterContents(register_name) = self.source {
            // MOV <reg>, <reg> opcode
            out.push(0x89);
            out.push(ModRmByte::from(ModRmAddressingMode::RegisterDirect, self.dest_register, Some(register_name)));
        } else {
            let value: u32 = match &self.source {
                DataSource::Literal(value) => *value as _,
                DataSource::NamedDataSymbol(symbol_name) => {
                    //println!("handling named symbol {symbol_name}");
                    let symbol_id = layout.get_symbol_id_for_symbol_name(symbol_name);
                    let symbol_type = layout.get_symbol_entry_type_for_symbol_name(symbol_name);
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
            out.push(ModRmByte::from(ModRmAddressingMode::RegisterDirect, self.dest_register, None));
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

pub struct Interrupt {
    id: InstructionId,
    vector: u8,
}

impl Interrupt {
    pub fn new(vector: u8) -> Self {
        Self {
            id: next_instruction_id(),
            vector,
        }
    }
}

impl Instruction for Interrupt {
    fn id(&self) -> InstructionId {
        self.id
    }

    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
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

#[derive(Debug, Clone)]
pub enum JumpTarget {
    Label(String),
}

pub struct Jump {
    id: InstructionId,
    target: JumpTarget,
}

impl Jump {
    pub fn new(target: JumpTarget) -> Self {
        Self {
            id: next_instruction_id(),
            target,
        }
    }
}

impl Instruction for Jump {
    fn id(&self) -> InstructionId {
        self.id
    }

    fn render(&self, layout: &FileLayout) -> Vec<u8> {
        let mut out = vec![];
        if let JumpTarget::Label(label_name) = &self.target {
            /*
            // JMP mem64
            // TODO(PT): Look for opportunities to use a JMP variant with a smaller encoded size
            // (i.e. if the named symbols is close by, use the JMP reloff8 variant)
            out.push(0xff);
            let symbol_id = layout.get_symbol_id_for_symbol_name(&label_name);
            let symbol_type = layout.get_symbol_entry_type_for_symbol_name(&label_name);
            let jump_target = if let SymbolEntryType::SymbolWithInlinedValue = symbol_type {
                layout.get_rebased_value(RebasedValue::ReadOnlyDataSymbolValue(symbol_id))
            } else {
                panic!("Unhandled symbol type {symbol_type:?}");
            };
            let mut jump_target_bytes = jump_target.to_le_bytes().to_owned().to_vec();
            out.append(&mut jump_target_bytes);
            */
            out.push(0xeb);
            out.push(0xfe);
        }
        out
    }
}

impl Display for Jump {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_fmt(format_args!("jmp {:?}", self.target))
    }
}

pub trait Instruction: Display {
    fn render(&self, layout: &FileLayout) -> Vec<u8>;
    fn id(&self) -> InstructionId;
}

pub type SymbolOffset = usize;
pub type SymbolSize = usize;

pub struct DataPacker {
    _file_layout: Rc<FileLayout>,
    pub symbols: Vec<(SymbolOffset, SymbolSize, Rc<DataSymbol>)>,
}

impl DataPacker {
    fn new(file_layout: &Rc<FileLayout>) -> Self {
        Self {
            _file_layout: Rc::clone(file_layout),
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

    pub fn _offset_of(self_: &Rc<RefCell<Self>>, sym: &Rc<DataSymbol>) -> usize {
        let this = self_.borrow();
        for (offset, _size, s) in this.symbols.iter() {
            if *s == *sym {
                return *offset;
            }
        }
        panic!("Failed to find symbol {sym}")
    }
}

pub struct InstructionPacker {
    _file_layout: Rc<FileLayout>,
    _data_packer: Rc<RefCell<DataPacker>>,
    instructions: RefCell<Vec<Rc<dyn Instruction>>>,
    rendered_instructions: RefCell<Vec<u8>>,
}

impl InstructionPacker {
    fn new(file_layout: &Rc<FileLayout>, data_packer: &Rc<RefCell<DataPacker>>) -> Self {
        Self {
            _file_layout: Rc::clone(file_layout),
            _data_packer: Rc::clone(data_packer),
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

    fn render(&self, _layout: &FileLayout) -> Vec<u8> {
        self.rendered_instructions.borrow().to_owned()
    }

    fn struct_type(&self) -> RebaseTarget {
        RebaseTarget::TextSection
    }
}

pub fn parse(layout: &Rc<FileLayout>, source: &str) -> (Rc<RefCell<DataPacker>>, Rc<InstructionPacker>) {
    // Generate code and data from source
    let lexer = AssemblyLexer::new(source);
    let mut parser = AssemblyParser::new(lexer);
    let (data_symbols, instructions) = parser.parse();
    println!("[### Assembly + ELF rendering ###]");

    // Render the data symbols
    let data_packer = Rc::new(RefCell::new(DataPacker::new(layout)));
    for data_symbol in data_symbols.iter() {
        DataPacker::pack(&data_packer, data_symbol);
    }

    // TODO(PT): We'll need a kind of Symbol that can be attached to a given instruction's address

    // Render the instructions
    let instruction_packer = Rc::new(InstructionPacker::new(layout, &data_packer));
    for instr in instructions.iter() {
        instruction_packer.pack(instr);
    }

    (data_packer, instruction_packer)
}
