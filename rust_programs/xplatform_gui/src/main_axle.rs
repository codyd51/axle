use agx_definitions::{Color, Point, Rect, Size};
use alloc::{rc::Rc, string::String};
use axle_rt::amc_register_service;
use axle_rt::println;
use libgui::text_input_view::TextInputView;
use libgui::ui_elements::UIElement;
use libgui::{view::View, AwmWindow};

pub fn main() {
    println!("Running in axle!");
    amc_register_service("com.user.xplatform_gui");
    let window_size = Size::new(1400, 900);
    let window = Rc::new(AwmWindow::new("IDE", window_size));

    let status_view_sizer = |superview_size: Size| {
        Rect::from_parts(Point::zero(), Size::new(superview_size.width, 100))
    };
    let source_code_view_sizer = move |superview_size| {
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

    let source_code_view =
        TextInputView::new(None, Size::new(16, 16), move |_v, superview_size| {
            source_code_view_sizer(superview_size)
        });
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

    window.enter_event_loop();
}
