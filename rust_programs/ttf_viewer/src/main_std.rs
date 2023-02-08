use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, NestedLayerSlice, Point, PointF64, Polygon,
    PolygonStack, Rect, RectInsets, Size, StrokeThickness,
};
use pixels::{Pixels, SurfaceTexture};
use ttf_renderer::{parse, Codepoint, Font, GlyphRenderDescription, GlyphRenderInstructions};
use winit::dpi::LogicalSize;
use winit::event::Event;
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;

use libgui::bordered::Bordered;
use libgui::text_input_view::TextInputView;
use libgui::ui_elements::UIElement;
use libgui::KeyCode;
use libgui::{view::View, AwmWindow};
use libgui_derive::{Drawable, NestedLayerSlice, UIElement};
use std::cmp::{max, min};
use std::rc::{Rc, Weak};
use std::time::Instant;
use std::{cmp, env, error, fs, io};

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(1200, 1200);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));

    let main_view_sizer = |superview_size: Size| Rect::from_parts(Point::zero(), superview_size);
    let main_view = TextInputView::new(move |_v, superview_size| main_view_sizer(superview_size));
    Rc::clone(&window).add_component(Rc::clone(&main_view) as Rc<dyn UIElement>);

    let mut main_view_slice = main_view.get_slice();

    let font_path = "/Users/philliptennen/Downloads/ASCII/ASCII.ttf";
    let font_path = "/System/Library/Fonts/Keyboard.ttf";
    let font_path = "/Users/philliptennen/Downloads/TestFont.ttf";
    let font_path = "/Users/philliptennen/Downloads/SF-Pro.ttf";
    let font_path = "/Users/philliptennen/Downloads/mplus1mn-bold-ascii.ttf";
    let font_path = "/System/Library/Fonts/Geneva.ttf";
    let font_path = "/Users/philliptennen/Downloads/nexa/NexaText-Trial-Regular.ttf";
    let font_path = "/System/Library/Fonts/Symbol.ttf";
    let font_path = "/System/Library/Fonts/NewYorkItalic.ttf";
    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);
    let font_size = Size::new(64, 64);

    //render_all_glyphs_in_font(&mut main_view_slice, &font, &font_size, None);
    render_string(
        &mut main_view_slice,
        &font,
        &font_size,
        "Sphinx_of_black_quartz,_judge_my_vow",
    );
    //render_glyph(&mut main_view_slice, &font.glyphs[8], 0.2, 0.2);

    window.enter_event_loop();
    Ok(())
}

fn render_all_glyphs_in_font(
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
        render_glyph(&mut dest_slice, glyph, scale_x, scale_y);
        cursor = Point::new(
            cursor.x + ((scaled_em_size.width as f64 * 1.0) as isize),
            cursor.y,
        );
        if cursor.x >= onto.frame().size.width - (font_size.width * 2) {
            cursor.y += scaled_em_size.height;
            cursor.x = cursor_origin.x;
        }
        println!("Rendered #{i}");
    }
}

fn render_string(onto: &mut Box<dyn LikeLayerSlice>, font: &Font, font_size: &Size, msg: &str) {
    let cursor_origin = Point::new(2, 2);
    let mut cursor = cursor_origin;
    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let scaled_em_size = Size::new(
        (font.bounding_box.size.width as f64 * scale_x) as isize,
        (font.bounding_box.size.height as f64 * scale_y) as isize,
    );
    for (i, ch) in msg.chars().enumerate() {
        let glyph = font.glyph_for_codepoint(Codepoint::from(ch)).unwrap();
        let mut dest_slice = onto.get_slice(Rect::from_parts(cursor, scaled_em_size));
        render_glyph(&mut dest_slice, glyph, scale_x, scale_y);
        cursor = Point::new(
            cursor.x + ((scaled_em_size.width as f64 * 0.38) as isize),
            cursor.y,
        );
        if cursor.x >= onto.frame().size.width - (font_size.width * 2) {
            cursor.y += scaled_em_size.height;
            cursor.x = cursor_origin.x;
        }
    }
}

fn render_glyph(
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
    let scaled_polygons: Vec<Polygon> = glyph
        .polygons
        .iter()
        .map(|p| p.scale_by(scale_x, scale_y))
        .collect();
    let polygon_stack = PolygonStack::new(&scaled_polygons);
    polygon_stack.fill(onto, Color::black());
}
