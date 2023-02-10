extern crate alloc;

use agx_definitions::{Color, LikeLayerSlice, Point, Polygon, PolygonStack, Rect, Size};
use alloc::boxed::Box;
use alloc::vec::Vec;
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
        render_glyph(&mut dest_slice, glyph, scale_x, scale_y);

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

pub fn render_glyph(
    onto: &mut Box<dyn LikeLayerSlice>,
    glyph: &GlyphRenderDescription,
    scale_x: f64,
    scale_y: f64,
) {
    /*
    for (i, polygon) in glyph.polygons.iter().enumerate() {
        let scaled_polygon = polygon.scale_by(scale_x, scale_y);
        let color = match i {
            0 => Color::red(),
            1 => Color::yellow(),
            _ => Color::green(),
        };
        //scaled_polygon.draw_outline(onto, Color::black());
        scaled_polygon.fill(onto, color);
    }
    */
    match &glyph.render_instructions {
        GlyphRenderInstructions::PolygonsGlyph(polygons_glyph) => {
            let scaled_polygons: Vec<Polygon> = polygons_glyph
                .polygons
                .iter()
                .map(|p| p.scale_by(scale_x, scale_y))
                .collect();
            let polygon_stack = PolygonStack::new(&scaled_polygons);
            polygon_stack.fill(onto, Color::black());
        }
        GlyphRenderInstructions::BlankGlyph(_blank_glyph) => {
            // Nothing to do
        }
        GlyphRenderInstructions::CompoundGlyph(compound_glyph) => {
            //onto.fill(Color::blue());
        }
    }
}
