use agx_definitions::{
    Color, Drawable, LayerSlice, LikeLayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::boxed::Box;
use alloc::rc::{Rc, Weak};
use axle_rt::println;
use libgui::text_view::TextView;
use libgui::{
    bordered::Bordered, label::Label, ui_elements::UIElement, view::View, window::KeyCode,
};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};

#[derive(NestedLayerSlice, Drawable, Bordered)]
pub struct OutputView {
    view: Rc<TextView>,
    title_label: Rc<Label>,
}

impl OutputView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(font_size: Size, sizer: F) -> Rc<Self> {
        let view = TextView::new(
            Color::new(30, 30, 30),
            font_size,
            // The top has extra insets to make room for the title label
            //RectInsets::new(8, 24, 8, 8),
            RectInsets::new(8, 8, 8, 8),
            sizer,
        );

        println!("Create title label");
        let title_label = Rc::new(Label::new(
            Rect::new(8, 8, 200, 16),
            "Title",
            Color::black(),
        ));
        println!("Add title label");
        //Rc::clone(&view.view).add_component(Rc::clone(&title_label) as Rc<dyn UIElement>);
        println!("Done adding title label");
        Rc::new(Self { view, title_label })
    }

    pub fn set_title(&self, title: &str) {
        self.title_label.set_text(title)
    }

    pub fn write(&self, text: &str) {
        for ch in text.chars() {
            self.view.draw_char_and_update_cursor(ch, Color::white());
        }
    }

    pub fn clear(&self) {
        self.view.clear()
    }
}

impl UIElement for OutputView {
    fn handle_mouse_entered(&self) {
        self.view.handle_mouse_entered()
    }

    fn handle_mouse_exited(&self) {
        self.view.handle_mouse_exited()
    }

    fn handle_mouse_moved(&self, mouse_point: Point) {
        self.view.handle_mouse_moved(mouse_point)
    }

    fn handle_left_click(&self, mouse_point: Point) {
        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        self.view.handle_key_pressed(key)
    }

    fn handle_key_released(&self, key: KeyCode) {
        self.view.handle_key_released(key)
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size);
        /*
        // Redraw the bar separating the title from the main content
        if self.get_parent().is_some() {
            self.view.get_slice().fill_rect(
                Rect::from_parts(Point::new(8, 24), Size::new(superview_size.width - 8, 1)),
                Color::dark_gray(),
                agx_definitions::StrokeThickness::Filled,
            );
        }
        */
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}
