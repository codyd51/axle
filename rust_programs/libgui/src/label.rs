use core::cell::RefCell;

use agx_definitions::{Color, Drawable, NestedLayerSlice, Point, Rect, Size};
use alloc::{
    rc::Weak,
    string::{String, ToString},
};

use crate::{font::draw_char, ui_elements::UIElement};

pub struct Label {
    // TODO(PT): Remove the nested RefCell?
    container: RefCell<Option<RefCell<Weak<dyn NestedLayerSlice>>>>,
    frame: Rect,
    pub text: RefCell<String>,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        Label {
            container: RefCell::new(None),
            frame,
            text: RefCell::new(text.to_string()),
            color,
        }
    }
    pub fn set_text(&self, text: &str) {
        self.text.replace(text.to_string());
    }
}

impl UIElement for Label {}

impl NestedLayerSlice for Label {
    fn get_parent(&self) -> Option<Weak<dyn NestedLayerSlice>> {
        Some(Weak::clone(
            &self.container.borrow().as_ref().unwrap().borrow(),
        ))
    }

    fn set_parent(&self, parent: Weak<dyn NestedLayerSlice>) {
        self.container.replace(Some(RefCell::new(parent)));
    }
}

impl Drawable for Label {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn content_frame(&self) -> Rect {
        Rect::from_parts(Point::zero(), self.frame().size)
    }

    fn draw(&self) {
        let onto = &mut self.get_slice();
        let font_size = Size::new(8, 10);
        let mut cursor = Point::zero();
        for ch in self.text.borrow().chars() {
            draw_char(onto, ch, &cursor, self.color, &font_size);
            cursor.x += font_size.width;
        }
    }
}
