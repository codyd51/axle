extern crate core;

mod parser;

use crate::parser::GlyphRenderDescription;
use parser::FontParser;

pub struct Font {
    pub name: String,
    pub glyph_render_descriptions: Vec<GlyphRenderDescription>,
}

impl Font {
    fn new(name: &str, glyph_render_descriptions: &Vec<GlyphRenderDescription>) -> Self {
        Self {
            name: name.to_string(),
            glyph_render_descriptions: glyph_render_descriptions.to_vec(),
        }
    }
}

pub fn parse(font_data: &[u8]) -> Font {
    let mut parser = FontParser::new(font_data);
    let glyph_render_descriptions = parser.parse();
    Font::new("abc", &glyph_render_descriptions)
}
