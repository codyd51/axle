use crate::parser::FontParser;
use crate::{println, Font};
use agx_definitions::Size;
use alloc::collections::VecDeque;
use alloc::vec;
use alloc::vec::Vec;

#[derive(Debug, Copy, Clone)]
enum Axis {
    X,
    Y,
}

#[derive(Debug, Copy, Clone)]
enum Zone {
    TwilightZone,
    GlyphZone,
}

impl From<u32> for Zone {
    fn from(value: u32) -> Self {
        match value {
            0 => Zone::TwilightZone,
            1 => Zone::GlyphZone,
            _ => panic!(),
        }
    }
}

#[derive(Debug, Copy, Clone)]
enum RoundStatePeriod {
    HalfPixel,
    Pixel,
    TwoPixels,
}

impl From<usize> for RoundStatePeriod {
    fn from(val: usize) -> Self {
        match val {
            0 => RoundStatePeriod::HalfPixel,
            1 => RoundStatePeriod::Pixel,
            2 => RoundStatePeriod::TwoPixels,
            _ => panic!("Invalid period value"),
        }
    }
}

#[derive(Debug, Copy, Clone)]
enum RoundStatePhase {
    Zero,
    QuarterPeriod,
    HalfPeriod,
    ThreeQuartersPeriod,
}

impl From<usize> for RoundStatePhase {
    fn from(val: usize) -> Self {
        match val {
            0 => RoundStatePhase::Zero,
            1 => RoundStatePhase::QuarterPeriod,
            2 => RoundStatePhase::HalfPeriod,
            3 => RoundStatePhase::ThreeQuartersPeriod,
            _ => panic!("Invalid phase value"),
        }
    }
}

#[derive(Debug, Copy, Clone)]
enum RoundStateThreshold {
    PeriodMinus1,
    MinusThreeEighthsTimesPeriod,
    MinusTwoEighthsTimesPeriod,
    MinusOneEighthTimesPeriod,
    Zero,
    OneEighthTimesPeriod,
    TwoEighthsTimesPeriod,
    ThreeEighthsTimesPeriod,
    FourEighthsTimesPeriod,
    FiveEighthsTimesPeriod,
    SixEighthsTimesPeriod,
    SevenEighthsTimesPeriod,
    Period,
    NineEighthsTimesPeriod,
    TenEighthsTimesPeriod,
    ElevenEighthsTimesPeriod,
}

impl From<usize> for RoundStateThreshold {
    fn from(val: usize) -> Self {
        match val {
            0 => RoundStateThreshold::PeriodMinus1,
            1 => RoundStateThreshold::MinusThreeEighthsTimesPeriod,
            2 => RoundStateThreshold::MinusTwoEighthsTimesPeriod,
            3 => RoundStateThreshold::MinusOneEighthTimesPeriod,
            4 => RoundStateThreshold::Zero,
            5 => RoundStateThreshold::OneEighthTimesPeriod,
            6 => RoundStateThreshold::TwoEighthsTimesPeriod,
            7 => RoundStateThreshold::ThreeEighthsTimesPeriod,
            8 => RoundStateThreshold::FourEighthsTimesPeriod,
            9 => RoundStateThreshold::FiveEighthsTimesPeriod,
            10 => RoundStateThreshold::SixEighthsTimesPeriod,
            11 => RoundStateThreshold::SevenEighthsTimesPeriod,
            12 => RoundStateThreshold::Period,
            13 => RoundStateThreshold::NineEighthsTimesPeriod,
            14 => RoundStateThreshold::TenEighthsTimesPeriod,
            15 => RoundStateThreshold::ElevenEighthsTimesPeriod,
            _ => panic!("Invalid threshold value"),
        }
    }
}

#[derive(Debug, Copy, Clone)]
struct RoundState {
    period: RoundStatePeriod,
    phase: RoundStatePhase,
    threshold: RoundStateThreshold,
}

impl RoundState {
    fn new(
        period: RoundStatePeriod,
        phase: RoundStatePhase,
        threshold: RoundStateThreshold,
    ) -> Self {
        Self {
            period,
            phase,
            threshold,
        }
    }
}

pub(crate) struct GraphicsState {
    font_size: Size,
    freedom_vector: Axis,
    projection_vector: Axis,
    interpreter_stack: VecDeque<u32>,
    zone_pointers: [Zone; 3],
    round_state: RoundState,
}

impl GraphicsState {
    pub(crate) fn new(font_size: Size) -> Self {
        Self {
            font_size,
            freedom_vector: Axis::X,
            projection_vector: Axis::X,
            interpreter_stack: VecDeque::new(),
            zone_pointers: [Zone::GlyphZone; 3],
            round_state: RoundState::new(
                RoundStatePeriod::Pixel,
                RoundStatePhase::Zero,
                RoundStateThreshold::FourEighthsTimesPeriod,
            ),
        }
    }

    fn push(&mut self, val: u32) {
        self.interpreter_stack.push_back(val)
    }

    fn pop(&mut self) -> u32 {
        self.interpreter_stack.pop_back().unwrap()
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
enum HintParseOperation {
    Print,
    Execute,
    IdentifyFunctions,
}

#[derive(Debug, Clone)]
pub(crate) struct HintParseOperations(Vec<HintParseOperation>);

impl HintParseOperations {
    fn should_print(&self) -> bool {
        self.0.contains(&HintParseOperation::Print)
    }

    fn should_execute(&self) -> bool {
        self.0.contains(&HintParseOperation::Execute)
    }

    pub(crate) fn debug_run() -> Self {
        Self(vec![HintParseOperation::Print, HintParseOperation::Execute])
    }

    pub(crate) fn identify_functions() -> Self {
        Self(vec![HintParseOperation::IdentifyFunctions])
    }
}

#[derive(Debug, Clone)]
pub struct FunctionDefinition {
    pub offset: usize,
    pub function_identifier: usize,
    pub instructions: Vec<u8>,
}

impl FunctionDefinition {
    fn new(offset: usize, function_identifier: usize, instructions: &[u8]) -> Self {
        Self {
            offset,
            function_identifier,
            instructions: instructions.to_vec(),
        }
    }
}

pub(crate) fn identify_functions(instructions: &[u8]) -> Vec<FunctionDefinition> {
    // There exists a bootstrapping problem in which functions can't be run to completion without
    // the context (arguments etc) from the caller, but we need to parse function boundaries before we can run
    // callers, since callers address functions by identifiers that are created when the fpgm table is executed.
    // To resolve this circular dependency, we use a simple heuristic to identify function definitions, rather
    // than doing a full interpreter pass. This has the downside that our heuristic can't tell the
    // difference between code and data. If some data is pushed to the stack with the same value as the
    // function definition opcode, we may interpret it as a function definition.
    let mut cursor = 0;
    let mut last_pushed_value: Option<u8> = None;
    let mut last_pushed_value_pc: Option<usize> = None;
    let mut identified_functions = vec![];
    loop {
        if cursor >= instructions.len() {
            break;
        }
        let opcode: &u8 = FontParser::read_data_with_cursor(instructions, &mut cursor);
        // Just handle PUSH[1] and FDEF here
        match opcode {
            0xb0 => {
                last_pushed_value_pc = Some(cursor);
                last_pushed_value = Some(*FontParser::read_data_with_cursor(
                    instructions,
                    &mut cursor,
                ));
            }
            0x2c => {
                // Ensure this directly followed a PUSH[1]
                assert_eq!(last_pushed_value_pc.unwrap() + 2, cursor);
                // TODO(PT) Limit to end func
                let function_instructions = &instructions[cursor..];
                identified_functions.push(FunctionDefinition::new(
                    cursor,
                    last_pushed_value.unwrap() as _,
                    function_instructions,
                ))
            }
            _ => (),
        }
    }
    identified_functions
}

pub(crate) fn parse_instructions(
    font: &Font,
    instructions: &[u8],
    operations: &HintParseOperations,
    graphics_state: &mut GraphicsState,
) {
    let mut cursor = 0;
    loop {
        let opcode: &u8 = FontParser::read_data_with_cursor(instructions, &mut cursor);
        if operations.should_print() {
            print!("{cursor:04x}\t{opcode:02x}\t");
        }
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
            0x13 => {
                // Set zone pointer 0
                let zone_number = graphics_state.pop();
                let zone = Zone::from(zone_number);
                if operations.should_print() {
                    println!("SZP0\tZone pointer 0 = {zone:?}");
                }
                if operations.should_execute() {
                    graphics_state.zone_pointers[0] = zone;
                }
            }
            0x23 => {
                // Swap
                if operations.should_print() {
                    println!("SWAP\ttop 2 stack elements");
                }
                println!("Stack: ");
                for x in graphics_state.interpreter_stack.iter().rev() {
                    println!("\t\t\t{x:08x}");
                }
                if operations.should_execute() {
                    let e2 = graphics_state.pop();
                    let e1 = graphics_state.pop();
                    graphics_state.push(e2);
                    graphics_state.push(e1);
                }
            }
            0x2b => {
                // Call
                // Function identifier number is popped from the stack
                let function_identifier_number = graphics_state.pop();
                if operations.should_print() {
                    println!("CALL #{function_identifier_number}");
                }
                if operations.should_execute() {
                    //
                }
            }
            0x2c => {
                // Function definition
                // Function identifier number is popped from the stack
                let function_identifier_number = graphics_state.pop();
                if operations.should_print() {
                    println!("Function define #{function_identifier_number}");
                }
                if operations.should_execute() {
                    //
                }
            }
            0x4b => {
                // Measure pixels per em in the projection vector's axis
                if operations.should_print() {
                    println!(
                        "MPPEM\tMeasure pixels per em in {:?}",
                        graphics_state.projection_vector
                    );
                }
                if operations.should_execute() {
                    let val = match graphics_state.projection_vector {
                        Axis::X => graphics_state.font_size.width,
                        Axis::Y => graphics_state.font_size.height,
                    };
                    graphics_state.push(val as u32);
                }
            }
            0x50 => {
                // Less than
                let e2 = graphics_state.pop();
                let e1 = graphics_state.pop();
                let result = e1 < e2;
                if operations.should_print() {
                    println!("LT\tLess than? {e1} < {e2} = {result}");
                }
                if operations.should_execute() {
                    graphics_state.push(if result { 1 } else { 0 });
                }
            }
            0x58 => {
                // If
                let condition = graphics_state.pop();
                let condition_passed = match condition {
                    0 => false,
                    1 => true,
                    _ => panic!("Invalid conditional flag"),
                };

                if operations.should_print() {
                    println!("IF\t{condition_passed}");
                }
                if operations.should_execute() {
                    if !condition_passed {
                        // Skip to the next ELSE
                        todo!()
                    }
                    // Otherwise, if the condition passed, drop through to the next instruction
                }
            }
            0x59 => {
                // Nothing to do
                if operations.should_print() {
                    println!("EIF");
                }
            }
            0x76 => {
                // Super round
                let val = graphics_state.pop();
                // > The threshold specifies the part of the domain, prior to a potential rounded value, that is mapped onto that value.
                let threshold_val = val & 0b111;
                let threshold = RoundStateThreshold::from(threshold_val as usize);
                // > The phase specifies the offset of the rounded values from multiples of the period.
                let phase_val = (val >> 3) & 0b11;
                let phase = RoundStatePhase::from(phase_val as usize);
                // > The period specifies the length of the separation or space between rounded values.
                let period_val = (val >> 5) & 0b11;
                let period = RoundStatePeriod::from(period_val as usize);

                if operations.should_print() {
                    println!("SROUND\tperiod={period:?}, phase={phase:?}, threshold={threshold:?}");
                }
                if operations.should_execute() {
                    graphics_state.round_state = RoundState::new(period, phase, threshold);
                }
                //
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
                    print!("Push {number_of_bytes_to_push} bytes:");
                    for byte in bytes_to_push.iter() {
                        print!("  {byte:02x}");
                    }
                    println!();
                }
                if operations.should_execute() {
                    for byte in bytes_to_push.iter() {
                        graphics_state.push(*byte as u32);
                    }
                }
            }
            _ => todo!("Unhandled opcode: 0x{opcode:02x}"),
        }
    }
}
