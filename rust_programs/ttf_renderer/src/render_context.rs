use crate::hints::{parse_instructions, GraphicsState, HintParseOperations};
use crate::Font;
use agx_definitions::Size;
use alloc::vec;
use alloc::vec::Vec;

pub struct FontRenderContext<'a> {
    font: &'a Font,
    font_size: Size,
    scaled_cvt: Vec<u32>,
    graphics_state: GraphicsState,
}

impl<'a> FontRenderContext<'a> {
    pub fn new(font: &'a Font, font_size: Size) -> Self {
        let mut graphics_state = GraphicsState::new(font_size);
        parse_instructions(
            font,
            &font.prep,
            &HintParseOperations::debug_run(),
            &mut graphics_state,
        );
        Self {
            font,
            font_size,
            scaled_cvt: vec![],
            graphics_state,
        }
    }
}
