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

use crate::font_viewer::FontViewer;
use crate::utils::{render_all_glyphs_in_font, render_glyph};
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

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(800, 600);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));
    let fonts = [
        "/Users/philliptennen/Downloads/ASCII/ASCII.ttf",
        "/Users/philliptennen/Downloads/TestFont.ttf",
        "/Users/philliptennen/Downloads/mplus1mn-bold-ascii.ttf",
        "/Users/philliptennen/Downloads/SF-Pro.ttf",
        "/Users/philliptennen/Downloads/pacifico/Pacifico.ttf",
        "/Users/philliptennen/Downloads/nexa/NexaText-Trial-Regular.ttf",
        "/Users/philliptennen/Downloads/roboto 2/Roboto-Medium.ttf",
        "/Users/philliptennen/Downloads/GeneralSans_Complete/Fonts/WEB/fonts/GeneralSans-Regular.ttf",
        "/System/Library/Fonts/NewYorkItalic.ttf",
        "/System/Library/Fonts/Keyboard.ttf",
        "/System/Library/Fonts/Symbol.ttf",
        "/System/Library/Fonts/Geneva.ttf",
        "/Users/philliptennen/Downloads/oswald/Oswald-Regular.ttf",
    ];
    let font_path = fonts.last().unwrap();
    Rc::new(RefCell::new(FontViewer::new(
        Rc::clone(&window),
        Some(font_path),
    )));
    window.enter_event_loop();
    Ok(())
}

pub fn main2() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(1200, 1200);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));

    let main_view_sizer = |superview_size: Size| Rect::from_parts(Point::zero(), superview_size);
    let main_view = TextInputView::new(None, Size::new(16, 16), move |_v, superview_size| {
        main_view_sizer(superview_size)
    });
    Rc::clone(&window).add_component(Rc::clone(&main_view) as Rc<dyn UIElement>);

    let mut main_view_slice = main_view.get_slice();

    let font_path = "/Users/philliptennen/Downloads/ASCII/ASCII.ttf";
    let font_path = "/Users/philliptennen/Downloads/TestFont.ttf";
    let font_path = "/Users/philliptennen/Downloads/mplus1mn-bold-ascii.ttf";
    let font_path = "/System/Library/Fonts/Geneva.ttf";
    let font_path = "/System/Library/Fonts/Symbol.ttf";
    let font_path = "/System/Library/Fonts/Keyboard.ttf";
    let font_path = "/Users/philliptennen/Downloads/SF-Pro.ttf";
    let font_path = "/System/Library/Fonts/NewYorkItalic.ttf";
    let font_path = "/Users/philliptennen/Downloads/pacifico/Pacifico.ttf";
    let font_path = "/Users/philliptennen/Downloads/GeneralSans_Complete/Fonts/WEB/fonts/GeneralSans-Regular.ttf";
    let font_path = "/Users/philliptennen/Downloads/roboto 2/Roboto-Medium.ttf";
    let font_path = "/Users/philliptennen/Downloads/nexa/NexaText-Trial-Regular.ttf";
    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);
    let font_size = Size::new(28, 28);

    //render_all_glyphs_in_font(&mut main_view_slice, &font, &font_size, Some(3000));
    render_string(
        &mut main_view_slice,
        &font,
        &font_size,
        "Sphinx of black quartz, judge my vow. This is a test of font rendering! I am trying out various glyphs and punctuations to see how they're handled. 1234567890!@#$%^&*()_+-=[]{}\\\'\"|,<.>/?§±`~",
        //"ij", //" ",
    );
    //render_glyph(&mut main_view_slice, &font.glyphs[2], 0.4, 0.4);
    let p1 = polygon_from_point_tups(&[
        (0, 20),
        (20, 10),
        (40, 20),
        (60, 10),
        (80, 20),
        (80, 30),
        (60, 20),
        (70, 30),
        (60, 40),
        (80, 40),
        (80, 50),
        (40, 40),
        (0, 50),
        (0, 40),
        (10, 40),
        (10, 30),
        (0, 30),
    ]);
    let p2 = polygon_from_point_tups(&[
        (30, 20),
        (20, 20),
        (15, 25),
        (15, 35),
        (25, 40),
        (40, 40),
        (30, 30),
    ]);
    let p3 = polygon_from_point_tups(&[(100, 100), (400, 100), (400, 400), (100, 400)]);

    //p1.fill(&mut main_view_slice, Color::red());
    //polygon.fill(&mut main_view_slice, Color::red());
    //let stack = PolygonStack::new(&[p1, p2]);
    //stack.fill(&mut main_view_slice, Color::red());

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
    for (i, ch) in msg.chars().enumerate() {
        let glyph = match font.glyph_for_codepoint(Codepoint::from(ch)) {
            None => continue,
            Some(glyph) => glyph,
        };
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
    }
}
