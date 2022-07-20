use core::{cell::RefCell, intrinsics};

use agx_definitions::{
    Color, Drawable, LayerSlice, NestedLayerSlice, Point, Rect, RectInsets, Size, StrokeThickness,
};
use alloc::{collections::BTreeMap, string::ToString, vec};
use alloc::{
    rc::{Rc, Weak},
    string::String,
    vec::Vec,
};
use axle_rt::println;
use libgui::{
    bordered::Bordered, button::Button, font::draw_char, label::Label, ui_elements::UIElement,
    view::View, window::KeyCode,
};

use crate::MessageHandler;

#[derive(Debug)]
struct CursorPos(usize, Point);

#[derive(Debug)]
struct DrawnCharacter {
    value: char,
    pos: Point,
    color: Color,
}

impl DrawnCharacter {
    fn new(pos: Point, color: Color, ch: char) -> Self {
        Self {
            value: ch,
            pos,
            color,
        }
    }
}

pub struct SourceCodeView {
    message_handler: Rc<MessageHandler>,
    view: Rc<View>,
    text: RefCell<Vec<DrawnCharacter>>,
    cursor_pos: RefCell<CursorPos>,
    is_shift_held: RefCell<bool>,
}

impl SourceCodeView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(
        message_handler: &Rc<MessageHandler>,
        sizer: F,
    ) -> Rc<Self> {
        let view = Rc::new(View::new(Color::white(), sizer));

        Rc::new(Self {
            message_handler: Rc::clone(message_handler),
            view,
            text: RefCell::new(vec![]),
            cursor_pos: RefCell::new(CursorPos(0, Point::zero())),
            is_shift_held: RefCell::new(false),
        })
    }

    pub fn get_text(&self) -> String {
        let mut out = vec![];
        let text = self.text.borrow();
        for ch in text.iter() {
            out.push(ch.value);
        }
        out.iter().collect()
    }

    fn text_entry_frame(&self) -> Rect {
        let content_frame = Bordered::content_frame(self);
        let inset_size = 8;
        content_frame.inset_by(inset_size, inset_size, inset_size, inset_size)
    }

    fn cursor_frame(&self) -> Rect {
        let cursor = (*self.cursor_pos.borrow()).1;
        Rect::from_parts(Point::new(cursor.x + 1, cursor.y - 1), Size::new(1, 14))
    }

    fn erase_cursor(&self) {
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let cursor_frame = self.cursor_frame();
        onto.fill_rect(cursor_frame, Color::white(), StrokeThickness::Filled);
    }

    fn draw_cursor(&self) {
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let cursor_frame = self.cursor_frame();
        onto.fill_rect(cursor_frame, Color::dark_gray(), StrokeThickness::Filled);
    }

    fn font_size(&self) -> Size {
        //Size::new(10, 14)
        Size::new(16, 16)
    }

    fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size();
        draw_char(onto, ch, &cursor_pos.1, color, &font_size);

        // TODO(PT): This is not correct if we're not inserting at the end
        // We'll need to adjust the positions of every character that comes after this one
        cursor_pos.0 += 1;
        let mut cursor_pos = &mut cursor_pos.1;

        self.text
            .borrow_mut()
            .push(DrawnCharacter::new(*cursor_pos, Color::black(), ch));

        if ch == '\n' || cursor_pos.x + (font_size.width * 2) >= onto.frame.width() {
            cursor_pos.x = 0;
            cursor_pos.y += font_size.height + 2;
        } else {
            cursor_pos.x += font_size.width;
        }
    }

    fn get_current_word(&self) -> String {
        let mut out = vec![];
        let mut cursor = self.cursor_pos.borrow().0;
        println!("Current cursor pos {cursor}");
        if cursor == 0 {
            return "".to_string();
        }
        let text = self.text.borrow();
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

    fn erase_char_and_update_cursor(&self) {
        let mut cursor_pos = self.cursor_pos.borrow_mut();
        let onto = &mut self.get_slice().get_slice(self.text_entry_frame());
        let font_size = self.font_size();

        if cursor_pos.0 == 0 {
            println!("Cannot delete with no text!");
            return;
        }

        let mut text = self.text.borrow_mut();
        let removed_char = text.remove(cursor_pos.0 - 1);

        cursor_pos.0 -= 1;
        let mut cursor_pos = &mut cursor_pos.1;
        *cursor_pos = removed_char.pos;

        onto.fill_rect(
            Rect::from_parts(removed_char.pos, font_size),
            Color::white(),
            StrokeThickness::Filled,
        );
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
        if word.starts_with("$") {
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
            .unwrap_or(Color::black());
        // TODO(PT): Only redraw on state transitions?
        for _ in 0..current_word.len() {
            self.erase_char_and_update_cursor();
        }
        for ch in current_word.chars() {
            self.draw_char_and_update_cursor(ch, word_color);
        }
    }

    fn put_char(&self, ch: char) {
        self.erase_cursor();
        self.draw_char_and_update_cursor(ch, Color::black());
        self.draw_cursor();
        self.recolorize_current_word();
    }

    fn delete_char(&self) {
        self.erase_cursor();
        self.erase_char_and_update_cursor();
        self.draw_cursor();
        self.recolorize_current_word();
    }

    fn set_cursor(&self, cursor_pos: CursorPos) {
        println!("Setting cursor to {cursor_pos:?}");
        self.erase_cursor();
        *self.cursor_pos.borrow_mut() = cursor_pos;
        self.draw_cursor();
    }
}

// TODO(PT): Model keycodes in Rust
const KEY_IDENT_LEFT_SHIFT: u32 = 0x995;
const KEY_IDENT_RIGHT_SHIFT: u32 = 0x994;

fn is_key_shift(key: KeyCode) -> bool {
    [KEY_IDENT_LEFT_SHIFT, KEY_IDENT_RIGHT_SHIFT].contains(&key.0)
}

impl UIElement for SourceCodeView {
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
        /*
        // Move the cursor to the closest character to the click point
        let distance = |p1: Point, p2: Point| {
            let x_dist = p2.x - p1.x;
            let y_dist = p2.y - p1.y;
            let dist_sum = x_dist.pow(2) + y_dist.pow(2);
            (unsafe { intrinsics::sqrtf64(dist_sum as f64) }) as isize
        };
        let text = self.text.borrow();
        let closest_char_to_click_point = text
            .iter()
            .enumerate()
            .min_by_key(|(i, ch)| distance(ch.1, mouse_point));
        if let Some((cursor_idx, closest_char_to_click_point)) = closest_char_to_click_point {
            // The char stores its origin, so we want to put the cursor just after it
            self.set_cursor(CursorPos(cursor_idx, closest_char_to_click_point.1));
        }
        */
        self.view.handle_left_click(mouse_point)
    }

    fn handle_key_pressed(&self, key: KeyCode) {
        if is_key_shift(key) {
            *self.is_shift_held.borrow_mut() = true;
        }

        if let Some(mut ch) = char::from_u32(key.0) {
            if ch < (128 as char) {
                if *self.is_shift_held.borrow() {
                    if ch.is_alphabetic() {
                        ch = ch.to_uppercase().next().unwrap();
                    }
                    let shifted_symbols = BTreeMap::from([
                        ('1', '!'),
                        ('2', '@'),
                        ('3', '#'),
                        ('4', '$'),
                        ('5', '%'),
                        ('6', '^'),
                        ('7', '&'),
                        ('8', '*'),
                        ('9', '('),
                        ('0', ')'),
                        ('-', '_'),
                        (';', ':'),
                        ('\'', '"'),
                        ('/', '?'),
                    ]);
                    ch = *shifted_symbols.get(&ch).unwrap_or(&ch);
                }

                if ch == '\t' {
                    for _ in 0..4 {
                        self.put_char(' ');
                    }
                // TODO(PT): Extract this constant (backspace)
                } else if ch == 0x08 as char {
                    println!("Caught backspace!");
                    self.delete_char();
                } else {
                    self.put_char(ch);
                }
            }
        } else {
            println!("Ignoring non-renderable character {key:?}");
        }
    }

    fn handle_key_released(&self, key: KeyCode) {
        if is_key_shift(key) {
            *self.is_shift_held.borrow_mut() = false;
        }
        //
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}

impl NestedLayerSlice for SourceCodeView {
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

impl Drawable for SourceCodeView {
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

impl Bordered for SourceCodeView {
    fn border_insets(&self) -> RectInsets {
        self.view.border_insets()
    }

    fn draw_inner_content(&self, outer_frame: Rect, onto: &mut LayerSlice) {
        self.view.draw_inner_content(outer_frame, onto);
    }
}
