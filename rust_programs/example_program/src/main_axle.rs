use alloc::boxed::Box;
use alloc::{collections::BTreeMap, format, rc::Weak, vec::Vec};
use alloc::{
    rc::Rc,
    string::{String, ToString},
};
use core::{cell::RefCell, cmp};

use libgui::bordered::Bordered;
use libgui::button::Button;
use libgui::label::Label;
use libgui::ui_elements::UIElement;
use libgui::view::View;
use libgui::AwmWindow;

use axle_rt::{amc_message_await, amc_message_send, amc_register_service, printf, AmcMessage};
use axle_rt::{ContainsEventField, ExpectsEventField};

use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, Line, NestedLayerSlice, Point, Rect, RectInsets,
    Size, StrokeThickness,
};

struct WindowState {
    pub window: Rc<AwmWindow>,
    content_view: Rc<View>,
}

impl WindowState {
    fn new(window: Rc<AwmWindow>, window_size: Size) -> Rc<Self> {
        let content_view = Rc::new(View::new(Color::white(), move |_view, superview_size| {
            Rect::from_parts(Point::zero(), superview_size)
        }));
        Rc::clone(&window).add_component(Rc::clone(&content_view) as Rc<dyn UIElement>);

        let label = Rc::new(Label::new(
            Rect::from_parts(Point::new(4, 4), Size::new(300, 30)),
            "Hello from Rust!",
            Color::black(),
        ));
        Rc::clone(&content_view).add_component(Rc::clone(&label) as Rc<dyn UIElement>);

        Rc::new(Self {
            window,
            content_view,
        })
    }
}

pub fn main() {
    amc_register_service("com.phillip.rust_window");
    let window_size = Size::new(400, 400);
    let window = Rc::new(AwmWindow::new("Rust GUI Toolkit", window_size));
    let dock = Rc::new(RefCell::new(WindowState::new(
        Rc::clone(&window),
        window_size,
    )));
    window.enter_event_loop();
}
