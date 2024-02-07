use agx_definitions::{
    Color, FillMode, LikeLayerSlice, NestedLayerSlice, Point, PointF64, Polygon, PolygonStack,
    Rect, Size, StrokeThickness,
};
use ttf_renderer::{parse, render_glyph_onto, Codepoint, Font};

use crate::font_viewer::FontViewer;
use libgui::font::load_font;
use libgui::label::Label;
use libgui::text_input_view::TextInputView;
use libgui::ui_elements::UIElement;
use libgui::AwmWindow;
use std::cell::RefCell;
use std::rc::Rc;
use std::{error, fs};

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
    let font_path = "/Users/philliptennen/Documents/fonts/helvetica-2.ttf";

    let font_size = Size::new(64, 64);
    let window_size = Size::new(1240, 1000);
    let window = Rc::new(AwmWindow::new("Hosted Font Viewer", window_size));

    let slice = window.get_slice();
    slice.fill(Color::white());
    let label = Label::new_with_font(
        //load_font(font_path)
        "tijpx",
        Color::black(),
        load_font(font_path),
        Size::new(64, 64),
        move |l, superview_size| Rect::from_parts(Point::new(16, 100), Size::new(200, 100)),
    );

    let main_view_sizer = |superview_size: Size| Rect::from_parts(Point::zero(), superview_size);
    let main_view = TextInputView::new(Some(font_path), font_size, move |_v, superview_size| {
        main_view_sizer(superview_size)
    });

    Rc::clone(&window).add_component(Rc::clone(&main_view) as Rc<dyn UIElement>);

    Rc::clone(&window).add_component(Rc::clone(&label) as Rc<dyn UIElement>);
    label.get_slice().fill(Color::red());

    for ch in "tijpx".chars() {
        main_view
            .view
            .draw_char_and_update_cursor(ch, Color::black());
    }

    slice.fill_polygon_stack(
        &PolygonStack::new(&[Polygon::new(&[
            PointF64::new(10.0, 10.0),
            PointF64::new(100.0, 10.0),
            PointF64::new(100.0, 100.0),
            PointF64::new(10.0, 100.0),
        ])]),
        Color::new(200, 80, 130),
        FillMode::Outline,
    );

    /*
    slice.fill_rect(
        Rect::new(10, 10, 90, 90),
        Color::red(),
        StrokeThickness::Filled,
    );

     */

    /*
    slice.fill_polygon_stack(
        &PolygonStack::new(&[Polygon::new(&[
            PointF64::new(0.0, 0.0),
            PointF64::new(100.0, 0.0),
            PointF64::new(100.0, 100.0),
            PointF64::new(0.0, 100.0),
        ])]),
        Color::green(),
        FillMode::Outline,
    );
    */

    //let mut main_view_slice = main_view.get_slice();

    let font_data = fs::read(font_path).unwrap();
    let font = parse(&font_data);
    //let font_render_context = FontRenderContext::new(&font, font_size);

    //render_all_glyphs_in_font(&mut main_view_slice, &font, &font_size, Some(3000));
    //render_string(&mut main_view_slice, &font, &font_size, "axle");
    /*
    render_string(
        &mut main_view_slice,
        &font,
        &font_size,
        &"abcdefghijklmnopqrstuvwzyz123456789".repeat(10),
    );
    */
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
        let (_, metrics) = render_glyph_onto(glyph, font, onto, cursor, Color::black(), *font_size);
        cursor = Point::new(cursor.x + (metrics.advance_width as isize), cursor.y);
        if cursor.x >= onto.frame().size.width - font_size.width {
            cursor.y += scaled_em_size.height;
            cursor.x = cursor_origin.x;
        }
    }
}
