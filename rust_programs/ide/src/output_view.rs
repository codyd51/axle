use agx_definitions::{
    Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::rc::{Rc, Weak};
use libgui::{bordered::Bordered, ui_elements::UIElement, view::View, window::KeyCode};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice, UIElement};

use crate::text_view::TextView;

#[derive(UIElement, NestedLayerSlice, Drawable, Bordered)]
pub struct OutputView {
    view: Rc<TextView>,
}

impl OutputView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        let view = TextView::new(Color::new(30, 30, 30), sizer);

        Rc::new(Self { view })
    }

    pub fn write(&self, text: &str) {
        for ch in text.chars() {
            self.view.draw_char_and_update_cursor(ch, Color::white());
        }
    }
}
