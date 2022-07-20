use agx_definitions::{
    Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::{
    format,
    rc::{Rc, Weak},
};
use axle_rt::println;
use libgui::{
    bordered::Bordered, button::Button, label::Label, ui_elements::UIElement, view::View,
};

use crate::MessageHandler;

pub struct StatusView {
    message_handler: Rc<MessageHandler>,
    view: Rc<View>,
    run_button: Rc<Button>,
    status_label: Rc<Label>,
}

impl StatusView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        message_handler: &Rc<MessageHandler>,
        sizer: F,
    ) -> Rc<Self> {
        let view = Rc::new(View::new(Color::light_gray(), sizer));

        let run_button = Rc::new(Button::new("Run", |_b, superview_size| {
            let size = Size::new(60, 30);
            Rect::from_parts(
                Point::new(10, superview_size.height - size.height - 10),
                size,
            )
        }));
        Rc::clone(&view).add_component(Rc::clone(&run_button) as Rc<dyn UIElement>);

        let status_label = Rc::new(Label::new(
            Rect::new(10, 10, 400, 16),
            "Status: Idle",
            Color::new(30, 30, 30),
        ));
        Rc::clone(&view).add_component(Rc::clone(&status_label) as Rc<dyn UIElement>);

        let ret = Rc::new(Self {
            message_handler: Rc::clone(message_handler),
            view,
            run_button: Rc::clone(&run_button),
            status_label,
        });

        let self_clone_for_button = Rc::clone(&ret);
        Rc::clone(&run_button).on_left_click(move |_b| {
            println!("Run button clicked!\n");
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

impl UIElement for StatusView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered();
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited();
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

impl NestedLayerSlice for StatusView {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        self.view.get_parent()
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.view.set_parent(parent);
    }

    fn get_slice(&self) -> LayerSlice {
        self.view.get_slice()
    }
}

impl Drawable for StatusView {
    fn frame(&self) -> Rect {
        self.view.frame()
    }

    fn content_frame(&self) -> Rect {
        Bordered::content_frame(self)
    }

    fn draw(&self) {
        Bordered::draw(self)
    }
}

impl Bordered for StatusView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}
