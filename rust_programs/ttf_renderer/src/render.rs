use crate::{Codepoint, Font, GlyphRenderDescription, GlyphRenderInstructions};
use agx_definitions::{Color, LikeLayerSlice, Point, Polygon, PolygonStack, Rect, Size};

pub fn render_char_onto(
    ch: char,
    font: &Font,
    onto: &mut Box<dyn LikeLayerSlice>,
    draw_loc: Point,
    draw_color: Color,
    font_size: Size,
) -> Rect {
    let codepoint = Codepoint::from(ch);
    let glyph = font.glyph_for_codepoint(codepoint);
    if glyph.is_none() {
        return Rect::zero();
    }
    let glyph = glyph.unwrap();
    render_glyph_onto(glyph, font, onto, draw_loc, draw_color, font_size)
}

pub fn render_glyph_onto(
    glyph: &GlyphRenderDescription,
    font: &Font,
    onto: &mut Box<dyn LikeLayerSlice>,
    draw_loc: Point,
    draw_color: Color,
    font_size: Size,
) -> Rect {
    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let scaled_glyph_metrics = glyph.metrics().scale(scale_x, scale_y);
    let draw_loc = draw_loc
        + Point::new(
            scaled_glyph_metrics.left_side_bearing,
            scaled_glyph_metrics.top_side_bearing,
        );
    let draw_box = Rect::from_parts(draw_loc, font_size);
    let mut dest_slice = onto.get_slice(draw_box);

    match &glyph.render_instructions {
        GlyphRenderInstructions::PolygonsGlyph(polygons_glyph) => {
            let scaled_polygons: Vec<Polygon> = polygons_glyph
                .polygons
                .iter()
                .map(|p| p.scale_by(scale_x, scale_y))
                .collect();
            let polygon_stack = PolygonStack::new(&scaled_polygons);
            polygon_stack.fill(&mut dest_slice, draw_color);
        }
        GlyphRenderInstructions::BlankGlyph(_blank_glyph) => {
            // Nothing to do
        }
        GlyphRenderInstructions::CompoundGlyph(compound_glyph) => {
            dest_slice.fill(Color::blue());
        }
    }

    Rect::from_parts(draw_box.origin, font_size)
}
