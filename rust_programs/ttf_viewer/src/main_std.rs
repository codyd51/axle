use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, LineF64, NestedLayerSlice, Point, PointF64, Polygon,
    PolygonStack, Rect, RectInsets, Size, StrokeThickness,
};
use pixels::{Pixels, SurfaceTexture};
use ttf_renderer::{
    parse, render_antialiased_glyph_onto, render_glyph_onto, Codepoint, Font,
    GlyphRenderDescription, GlyphRenderInstructions,
};
use winit::dpi::LogicalSize;
use winit::event::Event;
use winit::event_loop::{ControlFlow, EventLoop};
use winit::window::WindowBuilder;

use crate::font_viewer::FontViewer;
use crate::utils::render_all_glyphs_in_font;
use libgui::bordered::Bordered;
use libgui::text_input_view::TextInputView;
use libgui::ui_elements::UIElement;
use libgui::KeyCode;
use libgui::{view::View, AwmWindow};
use libgui_derive::{Drawable, NestedLayerSlice, UIElement};
use std::cell::RefCell;
use std::cmp::{max, min};
use std::rc::{Rc, Weak};
use std::time::Instant;
use std::{cmp, env, error, fs, io};

pub fn main2() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(800, 600);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));
    let fonts = ["/System/Library/Fonts/NewYorkItalic.ttf"];
    let font_path = fonts.last().unwrap();
    Rc::new(RefCell::new(FontViewer::new(
        Rc::clone(&window),
        Some(font_path),
    )));
    window.enter_event_loop();
    Ok(())
}

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let font_path = "/Users/philliptennen/Downloads/helvetica-2.ttf";

    let font_size = Size::new(16, 16);
    let window_size = Size::new(1240, 1000);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));

    let main_view_sizer = |superview_size: Size| Rect::from_parts(Point::zero(), superview_size);
    let main_view = TextInputView::new(Some(font_path), font_size, move |_v, superview_size| {
        main_view_sizer(superview_size)
    });
    Rc::clone(&window).add_component(Rc::clone(&main_view) as Rc<dyn UIElement>);

    let mut main_view_slice = main_view.get_slice();

    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);

    //render_all_glyphs_in_font(&mut main_view_slice, &font, &font_size, Some(3000));
    render_string(&mut main_view_slice, &font, &font_size, "axle");
    //render_glyph(&mut main_view_slice, &font.glyphs[2], 0.4, 0.4);

    window.enter_event_loop();
    Ok(())
}

fn polygon_from_point_tups(point_tups: &[(isize, isize)]) -> Polygon {
    let points = point_tups
        .iter()
        .map(|(x, y)| PointF64::new(*x as _, *y as _))
        .collect::<Vec<PointF64>>();
    Polygon::new(&points)
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
    for (_, ch) in msg.chars().enumerate() {
        let glyph = match font.glyph_for_codepoint(Codepoint::from(ch)) {
            None => continue,
            Some(glyph) => glyph,
        };
        let (_, metrics) =
            render_antialiased_glyph_onto(glyph, font, onto, cursor, Color::black(), *font_size);
        cursor = Point::new(cursor.x + (metrics.advance_width as isize), cursor.y);
        if cursor.x >= onto.frame().size.width - font_size.width {
            cursor.y += scaled_em_size.height;
            cursor.x = cursor_origin.x;
        }
    }
}
