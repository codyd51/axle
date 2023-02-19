use crate::parser::FontParser;
use crate::println;
use alloc::collections::VecDeque;
use alloc::vec;
use alloc::vec::Vec;

#[derive(Debug, Copy, Clone)]
enum Axis {
    X,
    Y,
}

pub(crate) struct GraphicsState {
    freedom_vector: Axis,
    projection_vector: Axis,
    interpreter_stack: VecDeque<u8>,
}

impl GraphicsState {
    fn new() -> Self {
        Self {
            freedom_vector: Axis::X,
            projection_vector: Axis::X,
            interpreter_stack: VecDeque::new(),
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum HintParseOperation {
    Print,
    Execute,
}

pub(crate) struct HintParseOperations(Vec<HintParseOperation>);

impl HintParseOperations {
    fn should_print(&self) -> bool {
        self.0.contains(&HintParseOperation::Print)
    }

    fn should_execute(&self) -> bool {
        self.0.contains(&HintParseOperation::Execute)
    }

    pub(crate) fn all() -> Self {
        Self(vec![HintParseOperation::Print, HintParseOperation::Execute])
    }
}

pub(crate) fn parse_instructions(instructions: &[u8], operations: HintParseOperations) {
    let mut cursor = 0;
    let mut graphics_state = GraphicsState::new();
    loop {
        let opcode: &u8 = FontParser::read_data_with_cursor(instructions, &mut cursor);
        println!("{cursor}: Got opcode {opcode:02x}");
        match opcode {
            0x00 | 0x01 => {
                // Set freedom and projection Vectors To Coordinate Axis
                let axis = match opcode {
                    0x00 => Axis::Y,
                    0x01 => Axis::X,
                    _ => panic!(),
                };

                if operations.should_print() {
                    println!("Set freedom and projection vectors to {axis:?}");
                }
                if operations.should_execute() {
                    graphics_state.projection_vector = axis;
                    graphics_state.freedom_vector = axis;
                }
            }
            0x2b => {
                // Call
                // Function identifier number is popped from the stack
                let function_identifier_number =
                    graphics_state.interpreter_stack.pop_back().unwrap();
                if operations.should_print() {
                    println!("CALL #{function_identifier_number}");
                }
                if operations.should_execute() {
                    //
                }
            }
            0xb0..=0xb7 => {
                // PUSH bytes
                let number_of_bytes_to_push = 1 + (opcode - 0xb0);
                let bytes_to_push: &[u8] = FontParser::read_bytes_from_data_with_cursor(
                    instructions,
                    &mut cursor,
                    number_of_bytes_to_push as usize,
                );
                if operations.should_print() {
                    println!("Push {number_of_bytes_to_push} bytes:");
                    for byte in bytes_to_push.iter() {
                        println!("\t{byte:02x}");
                    }
                }
                if operations.should_execute() {
                    for byte in bytes_to_push.iter() {
                        graphics_state.interpreter_stack.push_back(*byte);
                    }
                }
            }
            _ => todo!("{opcode}"),
        }
    }
}
