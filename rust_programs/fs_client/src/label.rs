use core::cell::RefCell;

use agx_definitions::{Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, Size};
use alloc::{
    rc::{Rc, Weak},
    string::{String, ToString},
};

use crate::{font::draw_char, ui_elements::UIElement};

pub struct Label {
    container: Option<Weak<dyn NestedLayerSlice>>,
    frame: Rect,
    text: RefCell<String>,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        Label {
            container: None,
            frame,
            text: RefCell::new(text.to_string()),
            color,
        }
    }
    pub fn set_text(&self, text: &str) {
        self.text.replace_with(|_| text.to_string());
    }
}

impl UIElement for Label {}

impl NestedLayerSlice for Label {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        Some(Weak::clone(self.container.as_ref().unwrap()))
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        todo!();
    }
}

impl Drawable for Label {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn draw(&self, onto: &mut LayerSlice) {
        let font_size = Size::new(8, 10);
        let mut cursor = Point::zero();
        for ch in self.text.borrow().chars() {
            draw_char(onto, ch, &cursor, self.color, &font_size);
            cursor.x += font_size.width;
        }
    }
}
