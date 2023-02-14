use crate::{Codepoint, Font, GlyphMetrics, GlyphRenderDescription, GlyphRenderInstructions};
use agx_definitions::{Color, LikeLayerSlice, Point, Polygon, PolygonStack, Rect, Size};
use alloc::boxed::Box;
use alloc::vec::Vec;

pub fn render_char_onto(
    ch: char,
    font: &Font,
    onto: &mut Box<dyn LikeLayerSlice>,
    draw_loc: Point,
    draw_color: Color,
    font_size: Size,
) -> (Rect, GlyphMetrics) {
    let codepoint = Codepoint::from(ch);
    let glyph = font.glyph_for_codepoint(codepoint);
    if glyph.is_none() {
        return (Rect::zero(), GlyphMetrics::zero());
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
) -> (Rect, GlyphMetrics) {
    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let scaled_glyph_metrics = glyph.metrics().scale(scale_x, scale_y);
    let draw_loc = draw_loc
        + Point::new(
            0, //scaled_glyph_metrics.left_side_bearing,
            //scaled_glyph_metrics.top_side_bearing,
            0,
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
            // Ensure at a minimum we see the outline points
            // This helps smooth over missing outlines for very thin/small glyphs
            /*
            for p in scaled_polygons.iter() {
                p.draw_outline(&mut dest_slice, draw_color);
            }
            */
            let polygon_stack = PolygonStack::new(&scaled_polygons);
            polygon_stack.fill(&mut dest_slice, draw_color);
        }
        GlyphRenderInstructions::BlankGlyph(_blank_glyph) => {
            // Nothing to do
        }
        GlyphRenderInstructions::CompoundGlyph(compound_glyph) => {
            //dest_slice.fill(Color::blue());
            for child_description in compound_glyph.children.iter() {
                let child_glyph = &font.glyphs[child_description.glyph_index];
                let origin = child_description.origin;
                let scaled_origin = Point::new(
                    (origin.x as f64 * scale_x) as isize,
                    (origin.y as f64 * scale_y) as isize,
                );
                //let subslice = dest_slice.get_slice(Rect::from_parts(scaled_origin), font_size);
                render_glyph_onto(
                    child_glyph,
                    font,
                    &mut dest_slice,
                    scaled_origin,
                    draw_color,
                    font_size,
                );
            }
        }
    }

    (
        Rect::from_parts(draw_box.origin, font_size),
        scaled_glyph_metrics,
    )
}
