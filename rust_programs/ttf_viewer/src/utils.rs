extern crate alloc;

use agx_definitions::{Color, LikeLayerSlice, Point, Polygon, PolygonStack, Rect, Size};
use alloc::boxed::Box;
use alloc::vec::Vec;
use libgui::font::draw_glyph_onto;
use ttf_renderer::{Font, GlyphRenderDescription, GlyphRenderInstructions};

pub fn render_all_glyphs_in_font(
    onto: &mut Box<dyn LikeLayerSlice>,
    font: &Font,
    font_size: &Size,
    limit: Option<usize>,
) {
    let cursor_origin = Point::new(2, 2);
    let mut cursor = cursor_origin;
    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let scaled_em_size = Size::new(
        (font.bounding_box.size.width as f64 * scale_x) as isize,
        (font.bounding_box.size.height as f64 * scale_y) as isize,
    );

    // Ref: https://stackoverflow.com/questions/29760668/conditionally-iterate-over-one-of-several-possible-iterators
    let mut iter1;
    let mut iter2;
    let glyphs_iter: &mut dyn Iterator<Item = (usize, &GlyphRenderDescription)> =
        if let Some(limit) = limit {
            iter1 = font.glyphs.iter().take(limit).enumerate();
            &mut iter1
        } else {
            iter2 = font.glyphs.iter().enumerate();
            &mut iter2
        };
    for (i, glyph) in glyphs_iter {
        let scaled_glyph_metrics = glyph.metrics().scale(scale_x, scale_y);
        let glyph_origin = Point::new(
            cursor.x + scaled_glyph_metrics.left_side_bearing,
            cursor.y + scaled_glyph_metrics.top_side_bearing,
        );
        let mut dest_slice = onto.get_slice(Rect::from_parts(glyph_origin, scaled_em_size));
        draw_glyph_onto(
            glyph,
            font,
            &mut dest_slice,
            Point::zero(),
            Color::black(),
            *font_size,
        );

        cursor = Point::new(
            cursor.x + (scaled_glyph_metrics.advance_width as isize),
            cursor.y,
        );
        if cursor.x >= onto.frame().size.width - (font_size.width * 2) {
            cursor.y += scaled_em_size.height;
            cursor.x = cursor_origin.x;
        }
        //println!("Rendered #{i}");
    }
}
