use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, NestedLayerSlice, Point, Polygon, Rect, RectInsets,
    Size, StrokeThickness,
};
use pixels::{Pixels, SurfaceTexture};
use ttf_renderer::parse;
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

    let font_path = "/Users/philliptennen/Downloads/nexa/NexaText-Trial-Regular.ttf";
    let font_path = "/System/Library/Fonts/Geneva.ttf";
    let font_path = "/System/Library/Fonts/Keyboard.ttf";
    let font_path = "/Users/philliptennen/Downloads/ASCII/ASCII.ttf";
    let font_path = "/Users/philliptennen/Downloads/SF-Pro.ttf";
    let font_path = "/Users/philliptennen/Downloads/mplus1mn-bold-ascii.ttf";
    let font_path = "/System/Library/Fonts/NewYorkItalic.ttf";
    let font_path = "/Users/philliptennen/Downloads/TestFont.ttf";
    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);
    let bounding_box = font.bounding_box;

    let mut cursor = Point::new(2, 2);
    let font_size = Size::new(30, 18);
    let scale_x = font_size.width as f64 / (font.units_per_em as f64);
    let scale_y = font_size.height as f64 / (font.units_per_em as f64);
    let scaled_em_size = Size::new(
        (font.bounding_box.size.width as f64 * scale_x) as isize,
        (font.bounding_box.size.height as f64 * scale_y) as isize,
    );

    let colors = [
        Color::black(),
        Color::blue(),
        Color::green(),
        Color::black(),
        Color::new(0, 255, 180),
        Color::yellow(),
        Color::light_gray(),
        Color::dark_gray(),
        Color::new(100, 20, 255),
        Color::new(200, 180, 140),
        Color::new(255, 100, 200),
        Color::new(80, 46, 100),
        Color::new(30, 240, 60),
    ];
    let s = "Dog_of_black_furs,_hear_the_vow";
    for ch in s.chars() {
        let codepoint = ch as u8 as usize;
        let glyph = font.codepoints_to_glyph_render_descriptions.get(&codepoint);
        if glyph.is_none() {
            continue;
        }
        let glyph = glyph.unwrap();
        let mut dest_slice = main_view_slice.get_slice(Rect::from_parts(cursor, scaled_em_size));
        for (j, polygon) in glyph.polygons.iter().enumerate() {
            // Flip Y
            let points: Vec<Point> = polygon
                .points
                .iter()
                .map(|&p| {
                    Point::new(
                        (p.x as f64 * scale_x) as _,
                        ((bounding_box.max_y() - p.y) as f64 * scale_y) as _,
                    )
                })
                .collect();
            let polygon = Polygon::new(&points);

            let start = Instant::now();
            //polygon.fill(&mut dest_slice, *colors.get(j).unwrap_or(&Color::black()));
            //polygon.fill(&mut dest_slice, Color::black());
            polygon.draw_outline(&mut dest_slice, Color::black());
            let duration = start.elapsed();
            println!("Glyph #{codepoint} polygon #{j} fill took {duration:?}");
        }
        cursor = Point::new(
            cursor.x + ((scaled_em_size.width as f64 * 0.35) as isize),
            cursor.y,
        );
        if cursor.x >= 1100 {
            cursor.y += scaled_em_size.height;
            cursor.x = 2;
        }
    }

    window.enter_event_loop();
    Ok(())
}
