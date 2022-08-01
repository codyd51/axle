use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::{
    boxed::Box,
    format,
    rc::{Rc, Weak},
};
use libgui::{
    bordered::Bordered, button::Button, label::Label, ui_elements::UIElement, view::View,
    window::KeyCode,
};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};

use crate::MessageHandler;

#[derive(UIElement, NestedLayerSlice, Drawable, Bordered)]
pub struct StatusView {
    message_handler: Rc<MessageHandler>,
    view: Rc<View>,
    _run_button: Rc<Button>,
    status_label: Rc<Label>,
}

impl StatusView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        message_handler: &Rc<MessageHandler>,
        sizer: F,
    ) -> Rc<Self> {
        let view = Rc::new(View::new(Color::new(180, 180, 180), sizer));

        let run_button = Rc::new(Button::new("Run", |_b, superview_size| {
            let size = Size::new(60, 30);
            Rect::from_parts(
                Point::new(10, superview_size.height - size.height - 10),
                size,
            )
        }));
        Rc::clone(&view).add_component(Rc::clone(&run_button) as Rc<dyn UIElement>);

        let status_label = Rc::new(Label::new(Rect::new(10, 10, 400, 16), "", Color::black()));
        Rc::clone(&view).add_component(Rc::clone(&status_label) as Rc<dyn UIElement>);

        let ret = Rc::new(Self {
            message_handler: Rc::clone(message_handler),
            view,
            _run_button: Rc::clone(&run_button),
            status_label,
        });

        let self_clone_for_button = Rc::clone(&ret);
        Rc::clone(&run_button).on_left_click(move |_b| {
            self_clone_for_button.set_status("Compiling...");
            self_clone_for_button
                .message_handler
                .publish(crate::Message::SendCompileRequest);
        });

        ret
    }

    pub fn set_status(&self, text: &str) {
        self.status_label.set_text(&format!("Status: {text}"));
        // Redraw the status view to reflect its new text
        Bordered::draw(&*self);
    }
}
