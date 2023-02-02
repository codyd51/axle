extern crate core;

mod parser;

use parser::FontParser;

pub struct Font {
    name: String,
}

impl Font {
    fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
        }
    }
}

pub fn parse(font_data: &[u8]) -> Font {
    let mut parser = FontParser::new(font_data);
    parser.parse();
    Font::new("abc")
}
