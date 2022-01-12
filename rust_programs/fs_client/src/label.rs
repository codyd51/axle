use agx_definitions::{Color, Drawable, LayerSlice, Rect, Size};
use alloc::string::{String, ToString};

use crate::ui_elements::UIElement;

pub struct Label {
    frame: Rect,
    pub text: String,
    color: Color,
}

impl Label {
    pub fn new(frame: Rect, text: &str, color: Color) -> Self {
        let max_size = Size::new(600, 480);
        Label {
            frame,
            text: text.to_string(),
            color,
        }
    }
}

impl UIElement for Label {}

impl Drawable for Label {
    fn frame(&self) -> Rect {
        self.frame
    }

    fn draw(&self, onto: &mut LayerSlice) {
        let font_size = Size::new(8, 8);
        let mut cursor = self.frame.origin;
        for ch in self.text.chars() {
            //draw_char(onto, ch, &cursor, self.color, &font_size);
            cursor.x += font_size.width;
        }
    }
}
