use core::cell::RefCell;

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
    bordered::Bordered, font::draw_char, ui_elements::UIElement, view::View, window::KeyCode,
};
use libgui_derive::{Bordered, Drawable, NestedLayerSlice};

#[derive(Debug)]
struct CursorPos(usize, Point);

#[derive(Debug)]
struct DrawnCharacter {
    value: char,
    pos: Point,
    _color: Color,
}

impl DrawnCharacter {
    fn new(pos: Point, color: Color, ch: char) -> Self {
        Self {
            value: ch,
            pos,
            _color: color,
        }
    }
}

/*
// TODO(PT): Good name for this?
// It's actually a customization point for the view controller
pub trait TextInputHandler {
    fn handle_key_pressed(&self, key: KeyCode);
    fn handle_key_released(&self, key: KeyCode);
}
*/

#[derive(Drawable, NestedLayerSlice, Bordered)]
pub struct TextInputView {
    view: Rc<View>,
    text: RefCell<Vec<DrawnCharacter>>,
    cursor_pos: RefCell<CursorPos>,
    is_shift_held: RefCell<bool>,
}

impl TextInputView {
    pub fn new<F: 'static + Fn(&View, Size) -> Rect>(sizer: F) -> Rc<Self> {
        let view = Rc::new(View::new(Color::white(), sizer));

        Rc::new(Self {
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

    pub fn draw_char_and_update_cursor(&self, ch: char, color: Color) {
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

    pub fn get_current_word(&self) -> String {
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

    pub fn erase_char_and_update_cursor(&self) {
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
        let cursor_pos = &mut cursor_pos.1;
        *cursor_pos = removed_char.pos;

        onto.fill_rect(
            Rect::from_parts(removed_char.pos, font_size),
            Color::white(),
            StrokeThickness::Filled,
        );
    }

    fn _set_cursor(&self, cursor_pos: CursorPos) {
        println!("Setting cursor to {cursor_pos:?}");
        self.erase_cursor();
        *self.cursor_pos.borrow_mut() = cursor_pos;
        self.draw_cursor();
    }

    fn put_char(&self, ch: char) {
        self.erase_cursor();
        self.draw_char_and_update_cursor(ch, Color::black());
        self.draw_cursor();
        //self.recolorize_current_word();
    }

    fn delete_char(&self) {
        self.erase_cursor();
        self.erase_char_and_update_cursor();
        self.draw_cursor();
        //self.recolorize_current_word();
    }
}

// TODO(PT): Model keycodes in Rust
const KEY_IDENT_LEFT_SHIFT: u32 = 0x995;
const KEY_IDENT_RIGHT_SHIFT: u32 = 0x994;

fn is_key_shift(key: KeyCode) -> bool {
    [KEY_IDENT_LEFT_SHIFT, KEY_IDENT_RIGHT_SHIFT].contains(&key.0)
}

impl UIElement for TextInputView {
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
    }

    fn handle_superview_resize(&self, superview_size: Size) {
        self.view.handle_superview_resize(superview_size)
    }

    fn currently_contains_mouse(&self) -> bool {
        self.view.currently_contains_mouse()
    }
}
