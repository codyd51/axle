use core::cell::RefCell;

use agx_definitions::{Color, Drawable, LayerSlice, Point, Rect, Size};
use alloc::string::{String, ToString};

use crate::{font::draw_char, ui_elements::UIElement};

pub struct Label {
    frame: Rect,
    text: RefCell<String>,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        Label {
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
