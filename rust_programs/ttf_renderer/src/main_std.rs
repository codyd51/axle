use agx_definitions::{
    Color, Drawable, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
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
use std::rc::{Rc, Weak};
use std::{env, error, fs, io};

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(1200, 1200);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));

    let main_view_sizer = |superview_size: Size| Rect::from_parts(Point::zero(), superview_size);
    let main_view = TextInputView::new(move |_v, superview_size| main_view_sizer(superview_size));
    Rc::clone(&window).add_component(Rc::clone(&main_view) as Rc<dyn UIElement>);

    let mut main_view_slice = main_view.get_slice();

    let font_path = "/Users/philliptennen/Downloads/TestFont.ttf";
    //let font_path = "/Users/philliptennen/Downloads/SF-Pro.ttf";
    let font_data = std::fs::read(font_path).unwrap();
    let font = parse(&font_data);
    let bounding_box = Rect::new(-5, -88, 1005, 928);

    let mut cursor = Point::new(20, 100);
    let scale = 128.0 / 1000.0; //units_per_em

    let glyph_indexes = [
        // Daisy
        61, 608, 724, 861, 940, //
        // /* space */ //
        2452, /* and */
        608, 786, 653,  // 'space'
        2452, // Phillip
        228, 708, 724, 756, 756, 724, 228, // space
        2452, 2452, //
        2522,
    ];
    //for glyph in font.glyph_render_descriptions.iter().take(32) {
    for &glyph_index in glyph_indexes.iter() {
        let glyph = &font.glyph_render_descriptions[glyph_index];
        let mut last_point = None;
        for &point in &glyph.points {
            // Flip Y
            let point = Point::new(
                (point.x as f64 * scale) as _,
                ((bounding_box.max_y() - point.y) as f64 * scale) as _,
            );
            if let Some(last_point) = last_point {
                let line = Line::new(cursor + last_point, cursor + point);
                line.draw(
                    &mut main_view_slice,
                    Color::red(),
                    StrokeThickness::Width(1),
                );
            }
            last_point = Some(point);
        }
        cursor = Point::new(cursor.x + 125, cursor.y);
        if cursor.x >= 1100 {
            cursor.y += 260;
            cursor.x = 20;
        }
    }

    window.enter_event_loop();
    Ok(())
}
