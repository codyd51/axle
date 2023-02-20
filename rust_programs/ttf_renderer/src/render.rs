use crate::hints::{parse_instructions, GraphicsState, HintParseOperations};
use crate::parser::FontParser;
use crate::println;
use crate::{Codepoint, Font, GlyphMetrics, GlyphRenderDescription, GlyphRenderInstructions};
use agx_definitions::{
    bounding_box_from_edges, scanline_compute_fill_lines_from_edges, Color, LikeLayerSlice, Point,
    PointF64, Polygon, PolygonStack, Rect, RectF64, Size, SizeF64, StrokeThickness,
};
use alloc::boxed::Box;
use alloc::vec;
use alloc::vec::Vec;
use num_traits::Float;
use num_traits::Zero;

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

pub fn render_antialiased_glyph_onto(
    glyph: &GlyphRenderDescription,
    font: &Font,
    onto: &mut Box<dyn LikeLayerSlice>,
    draw_loc: Point,
    draw_color: Color,
    font_size: Size,
) -> (Rect, GlyphMetrics) {
    // Upscale the font by 4x so we can supersample
    let ssaa_factor = 4;

    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let superscale_x = scale_x * ssaa_factor as f64;
    let superscale_y = scale_y * ssaa_factor as f64;

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
            let superscaled_polygons: Vec<Polygon> = polygons_glyph
                .polygons
                .iter()
                .map(|p| p.scale_by(superscale_x, superscale_y))
                .collect();
            let scaled_polygons: Vec<Polygon> = polygons_glyph
                .polygons
                .iter()
                .map(|p| p.scale_by(scale_x, scale_y))
                .collect();
            for p in scaled_polygons.iter() {
                p.draw_outline(&mut dest_slice, Color::yellow());
            }
            let superscaled_polygon_stack = PolygonStack::new(&superscaled_polygons);
            let superscaled_edges = superscaled_polygon_stack.lines();
            let superscaled_lines = scanline_compute_fill_lines_from_edges(&superscaled_edges);
            let superscaled_polygon_bounding_box = bounding_box_from_edges(&superscaled_lines);

            let scaled_polygon_stack = PolygonStack::new(&scaled_polygons);
            let scaled_edges = scaled_polygon_stack.lines();
            let scaled_polygon_bounding_box = bounding_box_from_edges(&scaled_edges);
            println!("Got superscaled edges bounding box {superscaled_polygon_bounding_box}");
            println!("Got      scaled edges bounding box {scaled_polygon_bounding_box}");
            let upscaled_bounding_box_width =
                superscaled_polygon_bounding_box.size.width.ceil() as usize;
            let upscaled_bounding_box_height =
                superscaled_polygon_bounding_box.size.height.ceil() as usize;
            let mut upscaled_bounding_box =
                vec![
                    vec![
                        false;
                        superscaled_polygon_bounding_box.origin.x.ceil() as usize
                            + upscaled_bounding_box_width
                    ];
                    superscaled_polygon_bounding_box.origin.y.ceil() as usize
                        + upscaled_bounding_box_height
                ];
            //println!("Scaled font size {scaled_font_size}");
            for line in superscaled_lines.iter() {
                assert_eq!(line.p1.y, line.p2.y, "Expect horizontal scanlines");
                let line_y = line.p1.y - superscaled_polygon_bounding_box.origin.y;
                //let line_y = line.p1.y;
                println!("Line {line}, origin {line_y}");
                for line_x in line.min_x().round() as usize..line.max_x().round() as usize {
                    upscaled_bounding_box[line_y as usize][line_x] = true;
                    onto.putpixel(
                        draw_loc + Point::new(line_x as isize, line_y as isize) + Point::new(0, 64),
                        Color::green(),
                    );
                }
            }

            let downscaled_width = scaled_polygon_bounding_box.size.width.ceil() as usize;
            let downscaled_height = scaled_polygon_bounding_box.size.height.ceil() as usize;
            //println!("downscaled width {downscaled_width} height {downscaled_height}");
            let mut downscaled_bounding_box = vec![vec![0.0; downscaled_width]; downscaled_height];
            println!("Font size {font_size}");

            // Compare every pixel with its neighbors
            for downscaled_y in 0..downscaled_height as usize {
                let upscaled_y: usize = downscaled_y * ssaa_factor as usize;
                for downscaled_x in 0..downscaled_width as usize {
                    let upscaled_x: usize = downscaled_x * ssaa_factor as usize;
                    println!("Downscaled ({downscaled_x}, {downscaled_y}), Upscaled ({upscaled_x}, {upscaled_y})");

                    let mut neighbors: Vec<bool> = vec![];
                    // Center pixel
                    neighbors.push(upscaled_bounding_box[upscaled_y][upscaled_x]);

                    if upscaled_y > 0 {
                        // Above pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y - 1][upscaled_x]);
                    }
                    if upscaled_y < upscaled_bounding_box_height - 1 {
                        // Below pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y + 1][upscaled_x]);
                    }
                    if upscaled_x > 0 {
                        // Left pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y][upscaled_x - 1]);
                    }
                    if upscaled_x < upscaled_bounding_box_width - 1 {
                        // Right pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y][upscaled_x + 1]);
                    }

                    if upscaled_y > 0 && upscaled_x > 0 {
                        // Top left pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y - 1][upscaled_x - 1]);
                    }

                    if upscaled_y > 0 && upscaled_x < upscaled_bounding_box_width - 1 {
                        // Top right pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y - 1][upscaled_x + 1]);
                    }

                    if upscaled_y < upscaled_bounding_box_height - 1 && upscaled_x > 0 {
                        // Bottom left pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y + 1][upscaled_x - 1]);
                    }

                    if upscaled_y < upscaled_bounding_box_height - 1
                        && upscaled_x < upscaled_bounding_box_width - 1
                    {
                        // Bottom right pixel
                        neighbors.push(upscaled_bounding_box[upscaled_y + 1][upscaled_x + 1]);
                    }

                    // Filled percentage
                    let fill_percentage = (neighbors.iter().filter(|&&v| v == true).count() as f64)
                        / (neighbors.len() as f64);
                    println!(
                        "\t\tFound {} neighbors with {:.2} fill",
                        neighbors.len(),
                        fill_percentage
                    );
                    downscaled_bounding_box[downscaled_y][downscaled_x] = fill_percentage;
                    // Debug: Upscale the antialiased pixels to make them easier to see
                    /*
                    if fill_percentage.is_zero() {
                        continue;
                    } else {
                        let color = color_lerp(Color::white(), Color::black(), fill_percentage);
                        let px =
                            Point::new(downscaled_x as isize, downscaled_y as isize) + draw_loc;
                        onto.fill_rect(
                            Rect::new(upscaled_x as _, upscaled_y as _, ssaa_factor, ssaa_factor),
                            color,
                            StrokeThickness::Filled,
                        );
                    }
                    */
                }
            }

            // Ensure at a minimum we see the outline points
            // This helps smooth over missing outlines for very thin/small glyphs
            for p in scaled_polygons.iter() {
                p.draw_outline(&mut dest_slice, draw_color);
            }
            let polygon_stack = PolygonStack::new(&scaled_polygons);
            polygon_stack.fill(&mut dest_slice, draw_color);
        }
        GlyphRenderInstructions::BlankGlyph(_blank_glyph) => {
            // Nothing to do
        }
        GlyphRenderInstructions::CompoundGlyph(compound_glyph) => {
            for child_description in compound_glyph.children.iter() {
                let child_glyph = &font.glyphs[child_description.glyph_index];
                let origin = child_description.origin;
                let scaled_origin = Point::new(
                    (origin.x as f64 * scale_x) as isize,
                    (origin.y as f64 * scale_y) as isize,
                );
                render_antialiased_glyph_onto(
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

fn render_polygons_glyph(
    glyph: &GlyphRenderDescription,
    font: &Font,
    font_size: Size,
    scale_x: f64,
    scale_y: f64,
    onto: &mut Box<dyn LikeLayerSlice>,
    draw_color: Color,
) {
    let polygons_description = match &glyph.render_instructions {
        GlyphRenderInstructions::PolygonsGlyph(polygons) => polygons,
        _ => panic!("Expected a polygons glyph"),
    };
    let scaled_polygons: Vec<Polygon> = polygons_description
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
    polygon_stack.fill(onto, draw_color);

    let instructions = glyph.hinting_program_bytes.as_ref().unwrap();
    let mut graphics_state = GraphicsState::new(font_size);
    parse_instructions(
        font,
        instructions,
        &HintParseOperations::debug_run(),
        &mut graphics_state,
    );
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
            //scaled_glyph_metrics.left_side_bearing,
            //scaled_glyph_metrics.top_side_bearing,
            0, 0,
        );
    let draw_box = Rect::from_parts(draw_loc, font_size);
    let mut dest_slice = onto.get_slice(draw_box);

    match &glyph.render_instructions {
        GlyphRenderInstructions::PolygonsGlyph(_) => {
            render_polygons_glyph(
                glyph,
                font,
                font_size,
                scale_x,
                scale_y,
                &mut dest_slice,
                draw_color,
            );
        }
        GlyphRenderInstructions::BlankGlyph(_blank_glyph) => {
            // Nothing to do
        }
        GlyphRenderInstructions::CompoundGlyph(compound_glyph) => {
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

fn lerp(a: f64, b: f64, percent: f64) -> f64 {
    a + (percent * (b - a))
}

fn color_lerp(color1: Color, color2: Color, percent: f64) -> Color {
    Color::new(
        lerp(color1.r as _, color2.r as _, percent).round() as _,
        lerp(color1.g as _, color2.g as _, percent).round() as _,
        lerp(color1.b as _, color2.b as _, percent).round() as _,
    )
}
