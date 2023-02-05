#![cfg_attr(target_os = "axle", no_std)]
#![feature(core_intrinsics)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]

extern crate alloc;
extern crate core;

mod parser;

use crate::parser::GlyphRenderDescription;
use agx_definitions::Rect;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use parser::FontParser;

pub struct Font {
    pub name: String,
    pub bounding_box: Rect,
    pub units_per_em: usize,
    pub codepoints_to_glyph_render_descriptions: BTreeMap<usize, GlyphRenderDescription>,
}

impl Font {
    fn new(
        name: &str,
        bounding_box: &Rect,
        units_per_em: usize,
        codepoints_to_glyph_render_descriptions: &BTreeMap<usize, GlyphRenderDescription>,
    ) -> Self {
        Self {
            name: name.to_string(),
            bounding_box: bounding_box.clone(),
            units_per_em,
            codepoints_to_glyph_render_descriptions: codepoints_to_glyph_render_descriptions
                .clone(),
        }
    }
}

pub fn parse(font_data: &[u8]) -> Font {
    let mut parser = FontParser::new(font_data);
    parser.parse()
    /*
    let glyph_render_descriptions = parser.parse();
    Font::new("abc", &glyph_render_descriptions)
    */
}
