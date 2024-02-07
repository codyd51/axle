#![cfg_attr(any(target_os = "axle", feature = "no_std"), no_std)]
#![feature(core_intrinsics)]
#![feature(slice_ptr_get)]
#![feature(format_args_nl)]

extern crate alloc;
extern crate core;

mod character_map;
mod glyphs;
mod hints;
mod metrics;
mod parse_utils;
mod parser;
mod render;
mod render_context;

pub use crate::glyphs::{GlyphRenderDescription, GlyphRenderInstructions};
pub use crate::metrics::GlyphMetrics;
pub use crate::render::{
    render_antialiased_glyph_onto, render_char_onto, render_glyph_onto, rendered_string_size,
};
pub use crate::render_context::FontRenderContext;
use agx_definitions::{Rect, Size};
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::fmt::{Display, Formatter};
use parser::FontParser;

use crate::hints::FunctionDefinition;
use crate::metrics::FontGlobalLayoutMetrics;
#[cfg(any(target_os = "axle", feature = "no_std"))]
pub(crate) use axle_rt::{print, println};
#[cfg(not(any(target_os = "axle", feature = "no_std")))]
pub(crate) use std::{print, println};

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

#[derive(Clone)]
pub struct GlyphIndex(usize);

#[derive(Clone)]
pub struct Font {
    pub name: String,
    pub bounding_box: Rect,
    pub units_per_em: usize,
    pub global_layout_metrics: FontGlobalLayoutMetrics,
    /// Sorted by glyph index
    pub glyphs: Vec<GlyphRenderDescription>,
    pub codepoints_to_glyph_indexes: BTreeMap<Codepoint, GlyphIndex>,
    pub functions_table: BTreeMap<usize, FunctionDefinition>,
    pub unscaled_cvt: Vec<u32>,
    pub prep: Vec<u8>,
}

impl Font {
    pub fn new(
        name: &str,
        bounding_box: &Rect,
        units_per_em: usize,
        global_layout_metrics: FontGlobalLayoutMetrics,
        glyphs: Vec<GlyphRenderDescription>,
        codepoints_to_glyph_indexes: BTreeMap<Codepoint, GlyphIndex>,
        functions_table: BTreeMap<usize, FunctionDefinition>,
        unscaled_cvt: Vec<u32>,
        prep: Vec<u8>,
    ) -> Self {
        Self {
            name: name.to_string(),
            bounding_box: bounding_box.clone(),
            units_per_em,
            global_layout_metrics,
            glyphs,
            codepoints_to_glyph_indexes,
            functions_table,
            unscaled_cvt,
            prep,
        }
    }

    pub fn glyph_for_codepoint(&self, codepoint: Codepoint) -> Option<&GlyphRenderDescription> {
        match self.codepoints_to_glyph_indexes.get(&codepoint) {
            None => None,
            Some(glyph_index) => self.glyphs.get(glyph_index.0),
        }
    }

    pub fn scaled_line_height(&self, font_size: Size) -> isize {
        let scale_factor = font_size.height as f64 / self.units_per_em as f64;
        // TODO(PT): A 'scale' method for the layout metrics as a whole, similar to the glyph metrics
        let scaled_ascent = self.global_layout_metrics.ascent as f64 * scale_factor;
        let scaled_descent = self.global_layout_metrics.descent as f64 * scale_factor;
        let scaled_line_gap = self.global_layout_metrics.line_gap as f64 * scale_factor;
        let scaled_line_height = scaled_ascent - scaled_descent + scaled_line_gap;
        scaled_line_height as isize
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
