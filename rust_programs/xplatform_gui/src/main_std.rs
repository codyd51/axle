use agx_definitions::{
    Color, Drawable, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
    StrokeThickness,
};
use libgui::bordered::Bordered;
use libgui::scroll_view::ScrollView;
use libgui::text_input_view::TextInputView;
use libgui::ui_elements::UIElement;
use libgui::KeyCode;
use libgui::{view::View, AwmWindow};
use libgui_derive::{Drawable, NestedLayerSlice, UIElement};
use std::rc::{Rc, Weak};
use std::{env, error, fs, io};

pub fn main() -> Result<(), Box<dyn error::Error>> {
    let window_size = Size::new(1400, 900);
    let window = Rc::new(AwmWindow::new("IDE", window_size));

    let status_view_sizer = |superview_size: Size| {
        Rect::from_parts(Point::zero(), Size::new(superview_size.width, 100))
    };
    let source_code_view_sizer = move |superview_size: Size| {
        let status_view_frame = status_view_sizer(superview_size);
        let usable_height = superview_size.height - status_view_frame.height();
        Rect::from_parts(
            Point::new(0, status_view_frame.max_y()),
            Size::new(
                ((superview_size.width as f64) * 0.4) as _,
                ((usable_height as f64) * 0.825) as isize,
                /*
                300 + 22,
                600 + 22,
                */
            ),
        )
        //Rect::new(00, 00, 250, 700)
    };
    let linker_output_sizer = move |superview_size| {
        let source_code_view_frame = source_code_view_sizer(superview_size);
        Rect::from_parts(
            Point::new(
                source_code_view_frame.width(),
                source_code_view_frame.min_y(),
            ),
            Size::new(
                superview_size.width - source_code_view_frame.width(),
                source_code_view_frame.height(),
            ),
        )
    };
    let program_output_view_sizer = move |superview_size| {
        let source_code_view_frame = source_code_view_sizer(superview_size);
        let usable_height = superview_size.height - source_code_view_frame.max_y();
        Rect::from_parts(
            Point::new(0, source_code_view_frame.max_y()),
            Size::new(superview_size.width, usable_height),
        )
    };

    let source_code_view = TextInputView::new(
        Some("/Users/philliptennen/Documents/develop/axle.nosync/axle-sysroot/fonts/sf_pro.ttf"),
        Size::new(16, 16),
        move |_v, superview_size| source_code_view_sizer(superview_size),
    );
    Rc::clone(&window).add_component(Rc::clone(&source_code_view) as Rc<dyn UIElement>);

    let top_view = Rc::new(View::new(Color::green(), move |_v, superview_size| {
        status_view_sizer(superview_size)
    }));
    Rc::clone(&window).add_component(Rc::clone(&top_view) as Rc<dyn UIElement>);
    let right_view = Rc::new(View::new(Color::blue(), move |_v, superview_size| {
        linker_output_sizer(superview_size)
    }));
    Rc::clone(&window).add_component(Rc::clone(&right_view) as Rc<dyn UIElement>);
    let bottom_view = Rc::new(View::new(Color::red(), move |_v, superview_size| {
        program_output_view_sizer(superview_size)
    }));
    Rc::clone(&window).add_component(Rc::clone(&bottom_view) as Rc<dyn UIElement>);

    /*
    let window = Rc::new(AwmWindow::new("XPlatform Window", Size::new(1000, 1000)));
    let view = Rc::new(BackedByView::new(View::new(Color::white(), move |_v, superview_size|{
        Rect::from_parts(Point::zero(), Size::new(300, 300))
    })));
    Rc::clone(&window).add_component(Rc::clone(&view) as Rc<dyn UIElement>);

    let scroll_view = Rc::new(BackedByScrollView::new(ScrollView::new(move |_v, superview_size|{
        Rect::from_parts(Point::new(00, 00), Size::new(400, 400))
    })));
    Rc::clone(&window).add_component(Rc::clone(&scroll_view) as Rc<dyn UIElement>);
    */

    window.enter_event_loop();
    Ok(())
}

#[derive(NestedLayerSlice, UIElement, Drawable)]
struct BackedByView {
    view: View,
}

impl BackedByView {
    fn new(view: View) -> Self {
        Self { view }
    }
}

impl Bordered for BackedByView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        onto.fill_rect(
            Rect::new(0, 0, 40, 40),
            Color::red(),
            StrokeThickness::Filled,
        );
        onto.draw_char('a', Point::zero(), Color::white(), Size::new(16, 16));
    }
}

#[derive(NestedLayerSlice, UIElement, Drawable)]
struct BackedByScrollView {
    view: Rc<ScrollView>,
}

impl BackedByScrollView {
    fn new(view: Rc<ScrollView>) -> Self {
        Self { view }
    }
}

impl Bordered for BackedByScrollView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut Box<dyn LikeLayerSlice>) {
        //self.view.layer.fill_rect(Rect::new(0, 0, 40, 40), Color::red(), StrokeThickness::Filled);
        //self.view.layer.draw_char('a', Point::zero(), Color::white(), Size::new(16, 16));
        //self.view.layer.draw_char(Rect::new(0, 0, 16, 16), 'a', Color::white());
        self.view
            .layer
            .draw_char(Rect::new(0, 292, 16, 16), 'a', Color::black());
        self.view
            .layer
            .draw_char(Rect::new(0, 300, 16, 16), 'a', Color::black());
        //onto.fill_rect(Rect::new(0, 0, 40, 40), Color::red(), StrokeThickness::Filled);
        //onto.draw_char('a', Point::zero(), Color::white(), Size::new(16, 16));
        self.view.layer.draw_visible_content_onto(onto);
    }
}
