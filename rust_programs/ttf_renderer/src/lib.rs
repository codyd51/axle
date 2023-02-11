#![cfg_attr(target_os = "axle", no_std)]
#![feature(core_intrinsics)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]

extern crate alloc;
extern crate core;

mod character_map;
mod glyphs;
mod metrics;
mod parse_utils;
mod parser;

pub use crate::glyphs::{GlyphRenderDescription, GlyphRenderInstructions};
pub use crate::metrics::GlyphMetrics;
use agx_definitions::Rect;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::fmt::{Display, Formatter};
use parser::FontParser;

#[cfg(target_os = "axle")]
pub(crate) use axle_rt::println;
#[cfg(not(target_os = "axle"))]
pub(crate) use std::println;

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Codepoint(usize);

impl From<char> for Codepoint {
    fn from(value: char) -> Self {
        Codepoint(value as u8 as usize)
    }
}

impl Display for Codepoint {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "Codepoint({})", self.0)
    }
}

pub struct GlyphIndex(usize);

pub struct Font {
    pub name: String,
    pub bounding_box: Rect,
    pub units_per_em: usize,
    /// Sorted by glyph index
    pub glyphs: Vec<GlyphRenderDescription>,
    pub codepoints_to_glyph_indexes: BTreeMap<Codepoint, GlyphIndex>,
}

impl Font {
    pub fn new(
        name: &str,
        bounding_box: &Rect,
        units_per_em: usize,
        glyphs: Vec<GlyphRenderDescription>,
        codepoints_to_glyph_indexes: BTreeMap<Codepoint, GlyphIndex>,
    ) -> Self {
        Self {
            name: name.to_string(),
            bounding_box: bounding_box.clone(),
            units_per_em,
            glyphs,
            codepoints_to_glyph_indexes,
        }
    }

    pub fn glyph_for_codepoint(&self, codepoint: Codepoint) -> Option<&GlyphRenderDescription> {
        match self.codepoints_to_glyph_indexes.get(&codepoint) {
            None => None,
            Some(glyph_index) => self.glyphs.get(glyph_index.0),
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
