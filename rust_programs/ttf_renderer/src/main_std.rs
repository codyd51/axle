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

use itertools::Itertools;
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

    //let font_path = "/Users/philliptennen/Downloads/SF-Pro.ttf";
    let font_path = "/Users/philliptennen/Downloads/ASCII/ASCII.ttf";
    let font_path = "/Users/philliptennen/Downloads/nexa/NexaText-Trial-Regular.ttf";
    let font_path = "/Users/philliptennen/Downloads/TestFont.ttf";
    //let font_path = "/Users/philliptennen/Downloads/mplus1mn-bold-ascii.ttf";
    let font_path = "/System/Library/Fonts/Geneva.ttf";
    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);
    let bounding_box = Rect::new(-5, -88, 1005, 928);

    /*
    let polygon = Polygon::new(&[
        Point::new(20, 250),
        Point::new(200, 20),
        Point::new(400, 200),
        Point::new(180, 100),
    ]);
    polygon.fill(&mut main_view_slice, Color::green());
    */
    /*
    let points = [
        (10, 00),
        (30, 20),
        (50, 00),
        (60, 10),
        (40, 30),
        (60, 50),
        (50, 60),
        (30, 40),
        (10, 60),
        (00, 50),
        (20, 30),
        (00, 10),
    ];
    let points: Vec<Point> = points.iter().map(|(x, y)| Point::new(*x, *y)).collect();
    let polygon = Polygon::new(&points);
    let start = Instant::now();
    polygon.fill(&mut main_view_slice, Color::green());
    let duration = start.elapsed();
    println!("Polygon fill took {duration:?}");

     */

    let mut cursor = Point::new(20, 10);
    let scale = 32.0 / 1000.0; //units_per_em

    let colors = [
        Color::red(),
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
    for (i, glyph) in font.glyph_render_descriptions.iter().enumerate() {
        for (j, polygon) in glyph.polygons.iter().enumerate() {
            // Flip Y
            let points: Vec<Point> = polygon
                .points
                .iter()
                .map(|&p| {
                    Point::new(
                        (p.x as f64 * scale) as _,
                        ((bounding_box.max_y() - p.y) as f64 * scale) as _,
                    )
                })
                .collect();
            let polygon = Polygon::new(&points);
            let mut dest_slice =
                main_view_slice.get_slice(Rect::from_parts(cursor, Size::new(100, 100)));

            let start = Instant::now();
            polygon.fill(&mut dest_slice, *colors.get(j).unwrap_or(&Color::black()));
            let duration = start.elapsed();
            println!("Glyph #{i} polygon #{j} fill took {duration:?}");

            cursor = Point::new(cursor.x + 40, cursor.y);
            if cursor.x >= 1100 {
                cursor.y += 40;
                cursor.x = 20;
            }
        }
    }

    window.enter_event_loop();
    Ok(())
}
