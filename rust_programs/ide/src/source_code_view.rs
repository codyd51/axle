use agx_definitions::{
    Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size,
};
use alloc::{collections::BTreeMap, string::ToString, vec};
use alloc::{
    rc::{Rc, Weak},
    string::String,
};
use axle_rt::println;
use libgui::{bordered::Bordered, ui_elements::UIElement, view::View, window::KeyCode};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};

use crate::{text_input_view::TextInputView, MessageHandler};

#[derive(NestedLayerSlice, Drawable, Bordered)]
pub struct SourceCodeView {
    _message_handler: Rc<MessageHandler>,
    view: Rc<TextInputView>,
}

impl SourceCodeView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        message_handler: &Rc<MessageHandler>,
        sizer: F,
    ) -> Rc<Self> {
        let view = TextInputView::new(sizer);
        Rc::new(Self {
            _message_handler: Rc::clone(message_handler),
            view,
        })
    }

    pub fn get_text(&self) -> String {
        self.view.get_text()
    }

    fn highlight_for_word(&self, word: &str) -> Option<Color> {
        let opcode_color = Color::new(160, 40, 211);
        let opcodes = vec!["mov", "int"];

        let keyword_color = Color::new(196, 149, 47);
        let keywords = vec![".global", ".section", ".ascii", ".equ", ".", "-"];

        let register_color = Color::new(47, 56, 245);
        let registers = vec![
            "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
        ];

        let colorize_keyword_sets = BTreeMap::from([
            (opcodes, opcode_color),
            (keywords, keyword_color),
            (registers, register_color),
        ]);
        let mut colorize_map = BTreeMap::new();
        for (keyword_set, color) in colorize_keyword_sets.iter() {
            for keyword in keyword_set.iter() {
                colorize_map.insert(keyword, *color);
            }
        }

        // $-prefixed values get colorized specially, as we can't do an exact match
        if word.starts_with('$') {
            return Some(Color::new(58, 145, 47));
        }

        // Option<&T> to Option<T>
        colorize_map.get(&word).cloned()
    }

    fn recolorize_current_word(&self) {
        // Un/colorize the current word
        let current_word = self.get_current_word();
        let word_color = self
            .highlight_for_word(&current_word)
            .unwrap_or_else(Color::black);
        // TODO(PT): Only redraw on state transitions?
        for _ in 0..current_word.len() {
            self.view.erase_char_and_update_cursor();
        }
        for ch in current_word.chars() {
            self.view.draw_char_and_update_cursor(ch, word_color);
        }
    }

    fn get_current_word(&self) -> String {
        let mut out = vec![];
        let mut cursor = self.view.get_cursor_pos().0;
        println!("Current cursor pos {cursor}");
        if cursor == 0 {
            return "".to_string();
        }
        let text = self.view.view.text.borrow();
        loop {
            let previous_ch = text[cursor - 1].value;
            if previous_ch.is_whitespace() || [','].contains(&previous_ch) {
                break;
            }
            out.push(previous_ch);

            cursor -= 1;

            if cursor == 0 {
                break;
            }
        }
        // We pushed the characters in reverse order
        out.reverse();
        out.iter().collect()
    }
}

impl UIElement for SourceCodeView {
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
        self.view.handle_key_pressed(key);
        self.recolorize_current_word()
    }

    fn handle_key_released(&self, key: KeyCode) {
        self.view.handle_key_released(key);
        self.recolorize_current_word()
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}
